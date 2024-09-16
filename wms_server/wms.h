#pragma once

// representations for bounding boxes and info on the server chunk partition

#include <stdint.h>
#include <string>
#include <memory>

#define CBOR_BBOX_BYTES 37
struct bbox {
    float minx;
    float miny;
    float maxx;
    float maxy;
};

// result returned from query to server about chunking resolution capabilities
// (& and other general server info to add as necessary?...)
#define CBOR_PARTITION_INFO_BYTES 3
// because we assume 3 byte packet length, bbox_per_deg must be 2 bytes ie. 2^16
#define MAX_ENCODABLE_BBOX_PER_DEG (1<<16)
struct partition_info {
    uint32_t bbox_per_deg;
};

void print_bbox(const struct bbox* query);

std::string get_bbox_filename(const struct bbox* query);

size_t create_normalized_bbox(const struct bbox* outer, std::unique_ptr<struct bbox[]>* arr);
