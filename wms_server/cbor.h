#pragma once

#include <memory>
#include <stdint.h>

#include <memory>
#include <nlohmann/json.hpp>

#include "wms.h"

// functions for encoding packets into binary format transmissibile between client & server
// we use CBOR to encode data in a binary format, try https://cbor.me/ to test what these binary
// packets will look like. NOTE cbor will encode integer types as the smallest possible size in
// which they fit, so the returned binaries may be smaller than actually is the case with larger
// integer values generated from code (eg. it can store values less than 2^16 as a half word, but
// if encoding a u32_t in general assume 5 bytes will be used: 4 for the word + 1 denoting its type)

// each packet is prefixed with a byte indicating its type
enum struct packet_type_enum: uint8_t {
    // recieved before a string of geojson packets, to indicate how many to expect
    PACKET_TYPE_GEOJSON_COUNT = 1,
    // geojson data for 1 chunk
    PACKET_TYPE_GEOJSON = 2,
    // query of a bounding box which repsesents a closed set of which the union of
    // returned chunks will be a closed superset
    PACKET_TYPE_BBOX = 3,
    // body-less request indicating client is asking for details on partition set up
    PACKET_TYPE_PARTITION_INFO_QUERY = 4,
    // info on how server has partitioned chunks (... & eventually other capabilities?)
    PACKET_TYPE_PARTITION_INFO = 5,
};

#define CBOR_HEADER_BYTES 12
struct packet_header {
    uint64_t payload_len;

    packet_type_enum type;
};

struct packet {
    struct packet_header header;
    std::unique_ptr<char[]> payload;
};

size_t encode_cbor_header(char* buf, size_t buf_size, const struct packet_header*header);
size_t decode_cbor_header(const char* buf, size_t buf_size, struct packet_header*header);

// functions for encoding & decoding various types of packet
// generally, these return 0 on success and an error code on failure

int encode_packet_bbox(const struct bbox* data, struct packet* packet);
int decode_packet_bbox(struct bbox* data, const struct packet* packet);

int encode_packet_geojson_count(uint64_t n, struct packet* packet);
int decode_packet_geojson_count(uint64_t* n, const struct packet* packet);

int encode_packet_geojson(const nlohmann::json & data, struct packet* packet);
nlohmann::json decode_packet_geojson(const struct packet* packet);

void encode_packet_partition_info_query(struct packet* packet);

int encode_packet_partition_info(struct packet* packet, const struct partition_info* p);
int decode_packet_partition_info(const struct packet* packet, struct partition_info* p);
