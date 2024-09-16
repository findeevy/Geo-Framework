#include <stdint.h>
#include <iostream>
#include <tinycbor/cbor.h>
#include <nlohmann/json.hpp>

#include "cbor.h"

using namespace std;
using json = nlohmann::json;

#define CHECK_ERR(...) do {                     \
        if ((__VA_ARGS__) != CborNoError) {     \
            return 0;                           \
        }                                       \
    } while(0)                                  \

size_t encode_cbor_header(char* buf, size_t buf_size, const struct packet_header*header) {
    CborEncoder enc, arrEnc;
    cbor_encoder_init(&enc, (uint8_t*)buf, buf_size, 0);
    CHECK_ERR(cbor_encoder_create_array(&enc, &arrEnc, 2));
    CHECK_ERR(cbor_encode_uint(&arrEnc, header->payload_len));
    CHECK_ERR(cbor_encode_uint(&arrEnc, (uint64_t)header->type));
    CHECK_ERR(cbor_encoder_close_container(&enc, &arrEnc));
    return cbor_encoder_get_buffer_size(&enc, (uint8_t*)buf);
}
size_t decode_cbor_header(const char* buf, size_t buf_size, struct packet_header*header) {
    CborParser par;
    CborValue val, arrVal;
    cbor_parser_init((const uint8_t*)buf, buf_size, 0, &par, &val);
    CHECK_ERR(cbor_value_enter_container(&val, &arrVal));
    CHECK_ERR(cbor_value_get_uint64(&arrVal, &header->payload_len));
    CHECK_ERR(cbor_value_advance(&arrVal));
    uint64_t tmp;
    CHECK_ERR(cbor_value_get_uint64(&arrVal, &tmp));
    assert(tmp < 0x100);
    header->type = (packet_type_enum)tmp;
    return buf_size;
}

size_t encode_bbox_cborbuf(uint8_t* buf, size_t size, const struct bbox* query) {
    CborEncoder enc, arrEnc;
    cbor_encoder_init(&enc, buf, size, 0);
    CHECK_ERR(cbor_encoder_create_array(&enc, &arrEnc, 4));
    CHECK_ERR(cbor_encode_float(&arrEnc, query->minx));
    CHECK_ERR(cbor_encode_float(&arrEnc, query->miny));
    CHECK_ERR(cbor_encode_float(&arrEnc, query->maxx));
    CHECK_ERR(cbor_encode_float(&arrEnc, query->maxy));
    CHECK_ERR(cbor_encoder_close_container(&enc, &arrEnc));
    return cbor_encoder_get_buffer_size(&enc, buf);
}
size_t decode_bbox_cborbuf(const uint8_t* buf, size_t size, struct bbox* query) {
    CborParser par;
    CborValue val, arrVal;
    cbor_parser_init(buf, size, 0, &par, &val);
    CHECK_ERR(cbor_value_enter_container(&val, &arrVal));
    CHECK_ERR(cbor_value_get_float(&arrVal, &query->minx));
    CHECK_ERR(cbor_value_advance(&arrVal));
    CHECK_ERR(cbor_value_get_float(&arrVal, &query->miny));
    CHECK_ERR(cbor_value_advance(&arrVal));
    CHECK_ERR(cbor_value_get_float(&arrVal, &query->maxx));
    CHECK_ERR(cbor_value_advance(&arrVal));
    CHECK_ERR(cbor_value_get_float(&arrVal, &query->maxy));
    return size;
}

int encode_packet_bbox(const struct bbox* data, struct packet* packet) {
    packet->header.type = packet_type_enum::PACKET_TYPE_BBOX;
    packet->payload = make_unique<char[]>(CBOR_BBOX_BYTES);
    size_t size = encode_bbox_cborbuf((uint8_t*)packet->payload.get(), CBOR_BBOX_BYTES, data);
    if (!size)
        return 1;
    packet->header.payload_len = size;
    return 0;
}

int decode_packet_bbox(struct bbox* data, const struct packet* packet) {
    assert(packet->header.type == packet_type_enum::PACKET_TYPE_BBOX);
    //assert(packet->header.payload_len >= CBOR_BBOX_BYTES);
    return !decode_bbox_cborbuf((uint8_t*)packet->payload.get(), packet->header.payload_len, data);
}

size_t encode_packet_geojson_count_cborbuf(uint8_t* buf, size_t size, uint64_t n) {
    CborEncoder enc;
    cbor_encoder_init(&enc, buf, size, 0);
    CHECK_ERR(cbor_encode_uint(&enc, n));
    return cbor_encoder_get_buffer_size(&enc, buf);
}

size_t decode_packet_geojson_count_cborbuf(const uint8_t* buf, size_t size, uint64_t* n) {
    CborParser par;
    CborValue val;
    cbor_parser_init(buf, size, 0, &par, &val);
    CHECK_ERR(cbor_value_get_uint64(&val, n));
    return size;
}

int encode_packet_geojson_count(uint64_t n, struct packet* packet) {
    packet->header.type = packet_type_enum::PACKET_TYPE_GEOJSON_COUNT;
    packet->payload = make_unique<char[]>(9);
    size_t size = encode_packet_geojson_count_cborbuf((uint8_t*)packet->payload.get(), 9, n);
    if (!size)
        return 1;
    packet->header.payload_len = size;
    return 0;
}

int decode_packet_geojson_count(uint64_t* n, const struct packet* packet) {
    if (packet->header.type != packet_type_enum::PACKET_TYPE_GEOJSON_COUNT)
        return -1;

    return !decode_packet_geojson_count_cborbuf((uint8_t*)packet->payload.get(), packet->header.payload_len, n);
}

int encode_packet_geojson(const json & data, struct packet* packet) {
    packet->header.type = packet_type_enum::PACKET_TYPE_GEOJSON;
    vector<uint8_t> v = json::to_cbor(data);
    if (!v.size())
        return 1;
    packet->payload = make_unique<char[]>(v.size());
    packet->header.payload_len = v.size();
    copy(v.begin(), v.end(), packet->payload.get());
    return 0;
}

json decode_packet_geojson(const struct packet* packet) {
    assert(packet->header.type == packet_type_enum::PACKET_TYPE_GEOJSON);
    vector<uint8_t> v(packet->payload.get(), packet->payload.get()+packet->header.payload_len);
    return json::from_cbor(v);
}

void encode_packet_partition_info_query(struct packet* packet) {
    packet->header.type = packet_type_enum::PACKET_TYPE_PARTITION_INFO_QUERY;
    packet->header.payload_len = 0;
}

size_t encode_packet_partition_info_cborbuf(uint8_t* buf, size_t size, const struct partition_info* p) {
    CborEncoder enc;
    cbor_encoder_init(&enc, buf, size, 0);
    CHECK_ERR(cbor_encode_uint(&enc, p->bbox_per_deg));
    return cbor_encoder_get_buffer_size(&enc, buf);
}

size_t decode_packet_partition_info_cborbuf(const uint8_t* buf, size_t size, struct partition_info* p) {
    CborParser par;
    CborValue val;
    cbor_parser_init(buf, size, 0, &par, &val);
    CHECK_ERR(cbor_value_get_int_checked(&val, (int*)&p->bbox_per_deg));
    return size;
}

int encode_packet_partition_info(struct packet* packet, const struct partition_info* p) {
    assert(p->bbox_per_deg < MAX_ENCODABLE_BBOX_PER_DEG);
    packet->header.type = packet_type_enum::PACKET_TYPE_PARTITION_INFO;
    packet->payload = make_unique<char[]>(CBOR_PARTITION_INFO_BYTES);
    size_t size = encode_packet_partition_info_cborbuf((uint8_t*)packet->payload.get(), CBOR_PARTITION_INFO_BYTES, p);

    if (!size)
        return 1;
    packet->header.payload_len = size;
    return 0;
}

int decode_packet_partition_info(const struct packet* packet, struct partition_info* p) {
    assert(packet->header.type == packet_type_enum::PACKET_TYPE_PARTITION_INFO);
    return !decode_packet_partition_info_cborbuf((uint8_t*)packet->payload.get(), packet->header.payload_len, p);
}
