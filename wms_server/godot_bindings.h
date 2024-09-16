#pragma once

// bindings for godot's c++ extension, allowing a godot client to connect to & communicate with
// a server

#include <memory>
#include <queue>
#include <mutex>
#include <thread>

#include <nlohmann/json.hpp>

// compile this file with -DNO_GODOT for testing with a mocked godot interface
// so it can be run in a seperate c++ client, without needing to interface with godot
#ifndef NO_GODOT
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/ref.hpp>
#endif

#include "sockpp/tcp_connector.h"
#include "wms.h"

// how many chunks around the player will be actively fetched from the server
// these should be the chunks the client attempts to render, or a superset of them
#define RENDER_DIST 1
// how many chunks around the player will not be actively fetched, but will remain in cached
// in memory if previously fetched
#define LAZY_DIST 2
// so with RENDER_DIST 1 and LAZY_DIST 2 it would look like this around the player (X):
// LLLLL
// LRRRL
// LRXRL
// LRRRL
// LLLLL

// derived values, LAZY_DIM is the total 1 dimensional length of the chunk store array
// see the example above: it is a 5x5 grid total because LAZY_DIM is 5
#define LAZY_DIM (LAZY_DIST * 2 + 1)
// & the total number elements
#define N_CHUNKS (LAZY_DIM * LAZY_DIM)

#ifdef NO_GODOT
typedef std::string String;
#endif

namespace godot {
    class GDClient
#ifndef NO_GODOT
        : public RefCounted
#endif
    {
#ifndef NO_GODOT
        GDCLASS(GDClient, RefCounted)
#endif
    private:
        // mutex for use of socket
        // because we expect a certain return type after making a request from the server,
        // we enforce a socket mutex so multiple threads can't interleave sent packets
        // nor fight over recieved ones
        std::mutex socket_mutex;
        sockpp::tcp_connector conn;
        std::string host;
        in_port_t port;
        // whether connected to the server
        bool connected;

        // storage for the local chunk cache; we protect all cache operations with a mutex
        // to prevent worker thread writing old values to cache after the player position has
        // been moved and so on
        std::mutex cache_mutex;
        std::unique_ptr<String> chunks[N_CHUNKS] = { NULL };
        uint32_t offx, offy;
        bool pos_set;
        int64_t chunkx, chunky;
        int part_res = -1;

        // the GDClient has a second worker thread responsible for issuing requests to the server
        // asynchronously, so the client does not lag waiting. to this end, a producer/consumer
        // queue of points to fetch is maintained, and once fetched the worker will attempt
        // to update the cache with fetched points (cache will not update if the player has since
        // moved so requested chunks are no longer loaded). the worker thread uses "chunk_loaded"
        // signal to notify the GODOT app that updates have been recieved
        std::thread fetch_handler;
        std::mutex fetch_queue_guard;
        std::queue<struct bbox> fetch_queue;
        // control boolean, can be set to false to halt the worker loop and allow the thread to
        // be joined
        std::atomic_bool run_thread = true;

        // internal function for sending & recieving actual packets to server for chunk info,
        // AFTER it has been verified the chunk is not already stored locally
        // note that this function also will not make any updates to the chunk cache after fetching
        // data; this function is totally cache-ignorant
        std::unique_ptr<nlohmann::json[]> get_chunk_info_unchecked(struct bbox bbox,
                                                                   uint64_t* nbb);

        // wraper around get_chunk_info_unchecked that will also check cache & update it after
        // recieving results
        std::unique_ptr<nlohmann::json[]> get_bbox_info(struct bbox bbox, uint64_t* nbb);

        // loop method for worker thread
        //
        // the GDClient will create and handle a second thread responsible for sending requests
        // to the connected geosdata server; after processing each request, the GDClient will
        // issue the "chunk_loaded" signal, which should be handled by godot.
        void spin_handle();

    protected:

#ifndef NO_GODOT
        // run by godot when loading plugin into interpreter,
        // marks which methods of the class can be accessed from GDScript
        // & can provide additional language server / behaviour info
        static void _bind_methods();
#endif

    public:
        GDClient();
        ~GDClient();

        // connects to server at host with port, and initializes data
        int connect_to_server(String host, in_port_t port);

        // disconnects from server, clearing local chunk cache and fetch queue
        void disconnect();

        // gets info on how chunks are partitioned
        // returns how many one (1) dimensional divisions are made to each unit length
        // of lattitude or longitude (division by unit area is this squared)
        //
        // eg. a result of 100 indicates chunk bounding boxes of 0.01 by 0.01 degrees
        int get_partition_info();

        // return whether a chunk is currently stored locally in cache
        bool has_chunk(float x, float y);

        // attempts to fetch info on the chunk which encloses the point provided
        // if the chunk is stored locally, it will return it,
        // otherwise it will attempt to query the server (this can take a while)
        // returns NULL if the chunk could not be fetched from the server
        //
        // NOTE: this is a blocking function; use with caution
        String get_chunk_info(float x, float y);

        // gets chunk info for locally stored chunk
        // if the chunk is not stored locally, will immediately return a NULL string
        String get_cached_chunk_info(float x, float y);

        // adds a chunk to the fetch queue; the chunk will be fetched asynchronously
        // and a signal will be sent to "chunk_loaded" once the queue entry has been dealt with
        //
        // note that if the client is unable to establish a connection to the server
        // or if the player center has moved before the request is processed, the "chunk_loaded"
        // signal may return NULL data. the client can reissue / ignore the request as needed
        void queue_fetch_chunk(float x, float y);

        // as above, but for an arbitrary bounding box that may result in any number or
        // chunk update signals being issued
        void queue_fetch_bbox(float minx, float miny, float maxx, float maxy);

        // updates the chunk store around a new player centre location
        // because x and y are rounded (floor) to the nearest chunk, values at the very edge
        // of the chunk should not be specified to avoid floating point rounding problems
        // this function will discard chunk values no longer in the lazy box around the player,
        // and attempt to fetch chunks in the render box around the player
        // fetching is done asynchronously and will send the "chunk_loaded" signal when done
        bool move_chunk_center(float x, float y);
    };
}

#ifndef NO_GODOT
// godot initialization functions
void initialize_module(godot::ModuleInitializationLevel p_level);
void uninitialize_module(godot::ModuleInitializationLevel p_level);
#endif
