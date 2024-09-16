#include <iostream>

#ifndef NO_GODOT
#include <gdextension_interface.h>

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#endif

#include "godot_bindings.h"
#include "cbor.h"
#include "socket.h"

using namespace std;
using namespace godot;
using json = nlohmann::json;

#define CLIENT_TIMEOUT 10s

// convert row and column indicies to actual array index (as an lval)
#define CHUNK_LVAL_RC(X, Y) (this->chunks[(X) + (Y) * LAZY_DIM])
// convert chunk x and y coordinates to element of chunk array
// chunk coords are lattitude / longitude coordinates multiplied by the bbox per degree resolution
// (from get_partition_info()) and rounded (floor) to ingegers
#define CHUNK_LVAL_UNCHECKED(X, Y)                                      \
    (CHUNK_LVAL_RC(((X) - this->chunkx + this->offx + LAZY_DIM) % LAZY_DIM, \
                   ((Y) - this->chunky + this->offy + LAZY_DIM) % LAZY_DIM))
// determine whether a requested chunk is close enough to the player position to be inside the cache
#define CHUNK_IS_STORED(X, Y)                                           \
    ((X) >= this->chunkx - LAZY_DIST && (X) <= this->chunkx + LAZY_DIST && \
     (Y) >= this->chunky - LAZY_DIST && (Y) <= this->chunky + LAZY_DIST)
// convert an index of the chunk array to the geospatial x chunk coordinate corresponding to
// the chunk at that position
//
// (eg. `chunks[i]` refers to a chunk with x coord 11.43; then `CHUNK_INDEX_TO_X(i)`
// returns `floor(11.43 * get_partition_info())` so maybe 1143 if resolution is 100)
#define CHUNK_INDEX_TO_X(I)                                             \
    ((((I) % LAZY_DIM) - this->offx + LAZY_DIST + LAZY_DIM) % LAZY_DIM - LAZY_DIST + this->chunkx)
// as above, for y coord
#define CHUNK_INDEX_TO_Y(I)                                             \
    ((((I) / LAZY_DIM) - this->offy + LAZY_DIST + LAZY_DIM) % LAZY_DIM - LAZY_DIST + this->chunky)
#define min(A, B) ({                            \
    __typeof__(A) _a = (A);                     \
    __typeof__(B) _b = (B);                     \
    _a < _b ? _a : _b;                          \
        })
#define max(A, B) ({                            \
    __typeof__(A) _a = (A);                     \
    __typeof__(B) _b = (B);                     \
    _a > _b ? _a : _b;                          \
        })

#ifndef NO_GODOT
// registration of which class methods can be accessed through godot
void GDClient::_bind_methods() {
    ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port"), &GDClient::connect_to_server);
    ClassDB::bind_method(D_METHOD("disconnect"), &GDClient::disconnect);
    ClassDB::bind_method(D_METHOD("get_partition_info"), &GDClient::get_partition_info);
    ClassDB::bind_method(D_METHOD("has_chunk", "x", "y"), &GDClient::has_chunk);
    ClassDB::bind_method(D_METHOD("get_chunk_info", "x", "y"), &GDClient::get_chunk_info);
    ClassDB::bind_method(D_METHOD("get_cached_chunk_info", "x", "y"), &GDClient::get_cached_chunk_info);
    ClassDB::bind_method(D_METHOD("queue_fetch_chunk", "x", "y"), &GDClient::queue_fetch_chunk);
    ClassDB::bind_method(D_METHOD("queue_fetch_bbox", "minx", "miny", "maxx", "maxy"), &GDClient::queue_fetch_bbox);
    ClassDB::bind_method(D_METHOD("move_chunk_center", "x", "y"), &GDClient::move_chunk_center);

    ADD_SIGNAL(MethodInfo("chunk_loaded", PropertyInfo(Variant::FLOAT, "x"), PropertyInfo(Variant::FLOAT, "y"), PropertyInfo(Variant::STRING, "data")));
}
#endif

// run loop for fetch worker thread
void GDClient::spin_handle() {
    while (run_thread) {
        this->fetch_queue_guard.lock();
        while (this->fetch_queue.empty()) {
            this->fetch_queue_guard.unlock();

            if (!run_thread) {
                return;
            }
            std::this_thread::yield();

            this->fetch_queue_guard.lock();
        }

        struct bbox pp = this->fetch_queue.front();
        this->fetch_queue.pop();
        this->fetch_queue_guard.unlock();

        printf("fetched point form work queue %f %f\n", pp.minx, pp.miny);

        uint64_t nbb;
        unique_ptr<json[]> res = this->get_bbox_info(pp, &nbb);

        if (res == NULL || nbb == 0) {
            return;
        }

        for (uint64_t i = 0; i < nbb; i++) {
#ifndef NO_GODOT
            emit_signal("chunk_loaded",
                        (float)res[i][0]["minx"],
                        (float)res[i][0]["miny"],
                        (String)res[i].dump().c_str());
#else
            printf("mocking godot signal emit:\tchunk_loaded, %f, %f, ...\n",
                   (float)res[i][0]["minx"],
                   (float)res[i][0]["miny"]);
#endif
        }
    }
}


GDClient::GDClient() {
}

GDClient::~GDClient() {
    this->run_thread = false;

    if (this->fetch_handler.joinable())
        this->fetch_handler.join();
}

int GDClient::connect_to_server(String host, in_port_t port) {
    this->socket_mutex.lock();

    if (connected) {
        this->socket_mutex.unlock();
        return -1;
    }

#ifndef NO_GODOT
    this->host = host.utf8().get_data();
#else
    this->host = host;
#endif
    this->port = port;

    pos_set = false;
    part_res = -1;

    sockpp::initialize();
    sockpp::result res = this->conn.connect(this->host, port, CLIENT_TIMEOUT);

    if (!res) {
        printf("could not connect:\n");
        printf("\t%s\n", res.error_message().c_str());
        this->socket_mutex.unlock();
        return 1;
    }

    connected = true;

    this->socket_mutex.unlock();

    this->fetch_handler = thread(&GDClient::spin_handle, this);

    return 0;
}

void GDClient::disconnect() {
    this->run_thread = false;
    if (this->fetch_handler.joinable())
        this->fetch_handler.join();

    this->socket_mutex.lock();
    pos_set = false;
    part_res = -1;

    if (connected) {
        this->conn.close();
    }
    connected = false;
    this->socket_mutex.unlock();

    this->fetch_queue_guard.lock();
    while (!this->fetch_queue.empty())
        fetch_queue.pop();
    this->fetch_queue_guard.unlock();
}

int GDClient::get_partition_info() {
    if (part_res > 0)
        return part_res;

    struct packet packet;
    encode_packet_partition_info_query(&packet);

    this->socket_mutex.lock();

    if (!connected) {
        this->socket_mutex.unlock();
        return -1;
    }

    if (send_packet(this->conn, &packet)) {
        this->socket_mutex.unlock();
        printf("sending packet returned error\n");
        return -1;
    }

    read_packet(this->conn, &packet);
    this->socket_mutex.unlock();

    struct partition_info p;
    if (packet.header.type != packet_type_enum::PACKET_TYPE_PARTITION_INFO
        || decode_packet_partition_info(&packet, &p)) {
        printf("could not decode packet as partition info\n");
        return -1;
    }

    part_res = p.bbox_per_deg;

    return p.bbox_per_deg;
    // json res = {{"bbox_per_deg", p.bbox_per_deg}};
    // return res;
}

unique_ptr<json[]> GDClient::get_chunk_info_unchecked(struct bbox bbox, uint64_t* nbb) {
    // struct bbox bbox = {.minx = x, .miny = y, .maxx = x, .maxy = y};
    *nbb = 0;

    struct packet packet;
    if (encode_packet_bbox(&bbox, &packet)) {
        printf("could not encode bbox packet\n");
        return NULL;
    }

    this->socket_mutex.lock();

    if (send_packet(this->conn, &packet)) {
        printf("could not send bbox packet\n");
        this->socket_mutex.unlock();
        return NULL;
    }

    read_packet(this->conn, &packet);

    if (packet.header.type == packet_type_enum::PACKET_TYPE_GEOJSON) {
        this->socket_mutex.unlock();

        json datj = decode_packet_geojson(&packet);
        *nbb = 1;

        unique_ptr<json[]> chunks = make_unique<json[]>(*nbb);
        chunks[0] = datj;
        return chunks;
    } else if (packet.header.type == packet_type_enum::PACKET_TYPE_GEOJSON_COUNT) {
        if (decode_packet_geojson_count(nbb, &packet)) {
            this->socket_mutex.unlock();

            printf("failed to decode geojson_count_packet\n");
            return NULL;
        }
        if (*nbb == 0) {
            this->socket_mutex.unlock();

            printf("server returned 0 chunks of geojson for query\n");
            return NULL;
        }

        unique_ptr<json[]> chunks = make_unique<json[]>(*nbb);

        for (uint64_t i = 0; i < *nbb; i++) {
            read_packet(this->conn, &packet);

            if (packet.header.type != packet_type_enum::PACKET_TYPE_GEOJSON) {
                this->socket_mutex.unlock();

                printf("expected GEOJSON, got %hhu\n", static_cast<uint8_t>(packet.header.type));
                return NULL;
            }

            json datj = decode_packet_geojson(&packet);
            chunks[i] = datj;
        }
        this->socket_mutex.unlock();

        return chunks;
    }
    this->socket_mutex.unlock();

    printf("expected GEOJSON or GEOJSON_COUNT, got %hhu\n",
           static_cast<uint8_t>(packet.header.type));
    return NULL;

}

String GDClient::get_chunk_info(float x, float y) {
    int res = get_partition_info();

    if (res < 0) {
        return (char*)NULL;
    }

    int checkx = floor(x * res), checky = floor(y * res);

    this->cache_mutex.lock();
    // if the request was for a single chunk, check if it was stored
    if (this->pos_set && CHUNK_IS_STORED(checkx, checky) && CHUNK_LVAL_UNCHECKED(checkx, checky)) {

        String c_val = *CHUNK_LVAL_UNCHECKED(checkx, checky);
        this->cache_mutex.unlock();

        printf("stored chunk found\n");
        return c_val;
    }
    this->cache_mutex.unlock();

    struct bbox bbox = { .minx = x,
                         .miny = y,
                         .maxx = x,
                         .maxy = y };

    uint64_t nbb;
    unique_ptr<json[]> v = get_bbox_info(bbox, &nbb);

    if (v == NULL || nbb == 0) {
        return (char*)NULL;
    }

    return (String)v[0].dump().c_str();
}

unique_ptr<json[]> GDClient::get_bbox_info(struct bbox bbox, uint64_t* nbb) {
    int res = get_partition_info();

    if (res < 0) {
        return NULL;
    }

    unique_ptr<json[]> v = get_chunk_info_unchecked(bbox, nbb);

    if (v == NULL || *nbb == 0) {
        return NULL;
    }

    this->cache_mutex.lock();
    for (uint64_t i = 0; i < *nbb; i++) {
        int checkx = round((float)v[i][0]["minx"] * res),
            checky = round((float)v[i][0]["miny"] * res);

        if (CHUNK_IS_STORED(checkx, checky)) {
            CHUNK_LVAL_UNCHECKED(checkx, checky) = make_unique<String>();
            *CHUNK_LVAL_UNCHECKED(checkx, checky) = (String)v[i].dump().c_str();
        }
    }

    this->cache_mutex.unlock();

    return v;
}

void GDClient::queue_fetch_bbox(float minx, float miny, float maxx, float maxy) {
    struct bbox pp = { .minx = minx,
                       .miny = miny,
                       .maxx = maxx,
                       .maxy = maxy };

    this->fetch_queue_guard.lock();
    this->fetch_queue.push(pp);
    this->fetch_queue_guard.unlock();

}

void GDClient::queue_fetch_chunk(float x, float y) {
    queue_fetch_bbox(x, y, x, y);
}

bool GDClient::has_chunk(float x, float y) {
    int res = get_partition_info();

    if (res < 0)
        return false;

    int checkx = floor(x * res), checky = floor(y * res);

    this->cache_mutex.lock();

    bool chunk_loaded = this->pos_set
        && CHUNK_IS_STORED(checkx, checky)
        && CHUNK_LVAL_UNCHECKED(checkx, checky);

    this->cache_mutex.unlock();

    return chunk_loaded;
}

String GDClient::get_cached_chunk_info(float x, float y) {
    int res = get_partition_info();

    if (res < 0)
        return (char*)NULL;

    int checkx = floor(x * res), checky = floor(y * res);

    this->cache_mutex.lock();

    if (!this->pos_set || !CHUNK_IS_STORED(checkx, checky)) {
        this->cache_mutex.unlock();
        return (char*)NULL;
    }

    if (CHUNK_LVAL_UNCHECKED(checkx, checky)) {
        String c_val = *CHUNK_LVAL_UNCHECKED(checkx, checky);
        this->cache_mutex.unlock();
        return c_val;
    }

    this->cache_mutex.unlock();
    return (char*)NULL;
}

bool GDClient::move_chunk_center(float xx, float yy) {
    int res = get_partition_info();

    if (res < 0) {
        printf("could not get chunk resolution\n");
        return false;
    }

    int newx = floor(xx * res), newy = floor(yy * res);


    this->cache_mutex.lock();
    if (pos_set && chunkx == newx && chunky == newy) {
        this->cache_mutex.unlock();
        return false;
    }
    printf("indeed setting center to %f %f\n", xx, yy);
    // check if there was a previous player position and the new position was close
    // enough that some old chunks can be retained
    if (pos_set && abs(newx - chunkx) < LAZY_DIM && abs(newy - chunky) < LAZY_DIM) {
        int minx = max(chunkx - LAZY_DIST, newx - LAZY_DIST),
            maxx = min(chunkx + LAZY_DIST, newx + LAZY_DIST),
            miny = max(chunky - LAZY_DIST, newy - LAZY_DIST),
            maxy = min(chunky + LAZY_DIST, newy + LAZY_DIST);

        for (int i = 0; i < N_CHUNKS; i++) {
            int x = CHUNK_INDEX_TO_X(i),
                y = CHUNK_INDEX_TO_Y(i);
            if (x < minx || x > miny || y < miny || y > maxy) {
                printf("got rid of chunk at (%d, %d)\n", x, y);
                chunks[i] = NULL;
            }

        }

        offx = (offx + newx - chunkx + LAZY_DIM) % LAZY_DIM;
        offy = (offy + newy - chunky + LAZY_DIM) % LAZY_DIM;

    } else {
        printf("clearing chunks store\n");
        // otherwise clear chunks of all stored info
        for (int i = 0; i < N_CHUNKS; i++) {
            chunks[i] = NULL;
        }
        // and reset offself values
        offx = LAZY_DIST;
        offy = LAZY_DIST;

        pos_set = true;
    }

    chunkx = newx;
    chunky = newy;

    for (int x = chunkx - RENDER_DIST; x <= chunkx + RENDER_DIST; x++) {
        for (int y = chunky - RENDER_DIST; y <= chunky + RENDER_DIST; y++) {
            if (CHUNK_LVAL_UNCHECKED(x, y) == NULL) {
                printf("loading chunk at (%d, %d)\n", x, y);

                struct bbox pp = { .minx = (float)(((double)x + 0.5) / (double)res),
                                   .miny = (float)(((double)y + 0.5) / (double)res),
                                   .maxx = (float)(((double)x + 0.5) / (double)res),
                                   .maxy = (float)(((double)y + 0.5) / (double)res) };

                this->fetch_queue_guard.lock();
                this->fetch_queue.push(pp);
                this->fetch_queue_guard.unlock();
            }
        }
    }

    this->cache_mutex.unlock();
    return true;
}


// ------------------------------------------------------------------------- //
// boilerplate initialization functions copied from godot c++ extension docs //
// ------------------------------------------------------------------------- //

#ifndef NO_GODOT
void initialize_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
		return;

	ClassDB::register_class<GDClient>();
}

void uninitialize_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
		return;
}

extern "C" {
    GDExtensionBool GDE_EXPORT library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_module);
        init_obj.register_terminator(uninitialize_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}
#endif
