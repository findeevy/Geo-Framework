#include <stdint.h>

#include "sockpp/tcp_acceptor.h"

#include "constants.h"
#include "cbor.h"

using namespace std;

int send_packet(sockpp::stream_socket & sock, const struct packet* packet) {
    char header[CBOR_HEADER_BYTES];
    size_t headed = encode_cbor_header(header, CBOR_HEADER_BYTES, &packet->header);

    if (!headed)
        return 1;

    sockpp::result<size_t> res;
    res = sock.write_n(header, CBOR_HEADER_BYTES);
    if (!res)
        return -1;
    // cout << "wrote header" << endl;

    if (packet->header.payload_len > 0) {
        res = sock.write_n(packet->payload.get(), packet->header.payload_len);
        if (!res)
            return -1;
        // cout << "wrote payload bytes " << packet->header.payload_len << endl;
    }

    return 0;
}

int read_packet(sockpp::stream_socket & sock, struct packet* packet) {
    char buf[CBOR_HEADER_BYTES];

    sockpp::result<size_t> res;
    res = sock.read_n(buf, CBOR_HEADER_BYTES);
    if (!res)
        return -1;

    size_t res_size = decode_cbor_header(buf, CBOR_HEADER_BYTES, &packet->header);
    if (!res_size)
        return 1;

    // cout << "read head, waiting to read " << packet->header.payload_len << endl;

    if (packet->header.payload_len > 0) {
        packet->payload = make_unique<char[]>(packet->header.payload_len);
        res = sock.read_n(packet->payload.get(), packet->header.payload_len);
        if (!res)
            return -1;
    }

    return 0;
}
