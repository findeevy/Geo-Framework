#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <atomic>

#include <gdal.h>
#include "sockpp/tcp_acceptor.h"

#include "wms_server/constants.h"
#include "wms_server/cbor.h"
#include "wms_server/osm_api.h"
#include "wms_server/chunk_manager.h"
#include "wms_server/socket.h"

// server keeps track of number of connected clients so threads created to manage connection to
// individual clients can be given the counter, allowing them to update when server has completed
// work relating to their connection counter
// NOTE: because we store which clients to update as a uint64_t bitvector, this puts a limit on
// # of connected clients at 64, but this could be changed easily by using like a uint64_t[N] for
// N * 64 max clients etc... or you could just use a Vector<uint64_t> and push each client to it,
// then iterate through the vector and send a message to each client...
// this was not done to avoid additional heap accesses (slow!)
uint8_t total_connection_count = 0;

using namespace std;
using json = nlohmann::json;

// main running thread for each connection to a client
void handler(sockpp::tcp_socket sock, const uint8_t connection_counter) {
    sockpp::result<size_t> res;

    while (1) {
        struct packet packet;
        if (read_packet(sock, &packet) != 0) {
            goto disconnect_label;
        }
        cout << "recieved packet with size: " << packet.header.payload_len << endl;
        switch(packet.header.type) {
        case packet_type_enum::PACKET_TYPE_BBOX: {
            struct bbox data;
            auto res = decode_packet_bbox(&data, &packet);
            assert(!res);
            cout << "packet decoded" << endl;
            print_bbox(&data);

            unique_ptr<struct bbox[]> bboxes;
            size_t local_stored;
            size_t nbb = load_bbox(&data, &bboxes, connection_counter, &local_stored);
            cout << "sending " << nbb << " bounding boxes" << endl;

            struct packet out_packet;
            res = encode_packet_geojson_count(nbb, &out_packet);
            assert(!res);
            if (send_packet(sock, &out_packet) != 0) {
                goto disconnect_label;
            }

            for (size_t i = 0; i < nbb; i++) {
                json geodata;
                if (i < local_stored) {
                    geodata = get_chunk_json_local(&bboxes[i]);
                } else {
                    geodata = get_chunk_json_workqueue(connection_counter);
                }

                auto res = encode_packet_geojson(geodata, &out_packet);
                assert(!res);
                if (send_packet(sock, &out_packet) != 0) {
                    goto disconnect_label;
                }
            }
            break;
        }
        case packet_type_enum::PACKET_TYPE_PARTITION_INFO_QUERY: {
            struct partition_info p;
            p.bbox_per_deg = BBOX_PER_DEG_INT;
            // TODO? &c.....

            struct packet out_packet;
            auto res = encode_packet_partition_info(&out_packet, &p);
            assert(!res);
            if (send_packet(sock, &out_packet) != 0)
                goto disconnect_label;

            break;
        }
        default:
            cout << "recieved unexpected packet type " << (int)packet.header.type << endl;
            continue;
        }
    }
 disconnect_label:
    printf("client %hhu disconnecting...\n", connection_counter);
}

// server will create a worker thread (responsible for querying osm, see chunk_manager.cpp),
// and then sit idle waiting for connections,
// dispatching new threads to manage each connection as clients connect
int main() {
    in_port_t port = SERVER_PORT;
    sockpp::initialize();

    error_code ec;
    sockpp::tcp_acceptor acc{port, 4, ec};

    if (ec) {
        cout << ec.message() << endl;
        exit(1);
    }

    GDALAllRegister();
    start_worker_thread(&total_connection_count);

    cout << "waiting for connection on " << port << endl;

    while (1) {
        sockpp::inet_address peer;

        sockpp::result res = acc.accept(&peer);

        if (res) {
            if (total_connection_count >= MAX_CLIENTS) {
                cout << "error: too many clients" << endl; // we are hard limited at 64 until we
                                                           // change the thread notificaiton
                                                           // bitvector into something scalable
                                                           // (uint64_t threads in bbox_task)
                continue;
            }
            cout << "connection with " << peer << endl;
            sockpp::tcp_socket sock = res.release();

            thread thr(handler, std::move(sock), total_connection_count++);
            thr.detach();
        } else
            cout << "error " << res.error_message() << endl;
    }

    end_worker_thread();
}
