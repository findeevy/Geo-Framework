#include <iostream>
#include <format>
#include <cmath>
#include <memory>
#include <stdint.h>

#include "wms.h"
#include "constants.h"

using namespace std;

void print_bbox(const struct bbox* query) {
    cout << "getmap\n\tminx: " << query->minx
         << "\n\tminy: " << query->miny
         << "\n\tmaxx: " << query->maxx
         << "\n\tmaxy: " << query->maxy << endl;
}

// file base for various features corresponding to a specific bounding box
// generally files will then be suffixed ased on feature information eg.
// map_bbox_0_0_1_1_points.geojson would be a complete file name
string get_bbox_filename(const struct bbox* query) {
    return format("map_bbox_{}_{}_{}_{}",
                  round(query->minx * BBOX_PER_DEG),
                  round(query->miny * BBOX_PER_DEG),
                  round(query->maxx * BBOX_PER_DEG),
                  round(query->maxy * BBOX_PER_DEG));
}

// splits a bbox into a grid of regularly sized bounding boxes, as defined by the server partition
// resolution. NOTE set of gridded bboxes will always be a superset of the provided user input,
// so every point in the outer bbox will be returned, but if it partially fills a square of the
// partition grid, the whole grid tile will be returned
size_t create_normalized_bbox(const struct bbox* outer, unique_ptr<struct bbox[]>* arr) {
    float minx = floor(outer->minx * BBOX_PER_DEG) / BBOX_PER_DEG;
    float miny = floor(outer->miny * BBOX_PER_DEG) / BBOX_PER_DEG;
    float maxx = (floor(outer->maxx * BBOX_PER_DEG) + 1) / BBOX_PER_DEG;
    float maxy = (floor(outer->maxy * BBOX_PER_DEG) + 1) / BBOX_PER_DEG;

    size_t bboxes_x = (size_t)round(abs(maxx-minx) * BBOX_PER_DEG),
        bboxes_y = (size_t)round(abs(maxy-miny) * BBOX_PER_DEG);

    *arr = make_unique<struct bbox[]>(bboxes_x * bboxes_y);

    for (size_t x = 0; x < bboxes_x; x++)
        for (size_t y = 0; y < bboxes_y; y++)
            (*arr)[x * bboxes_y + y] = (struct bbox){
                .minx = minx + (float)x / BBOX_PER_DEG,
                .miny = miny + (float)y / BBOX_PER_DEG,
                .maxx = minx + (float)(x+1) / BBOX_PER_DEG,
                .maxy = miny + (float)(y+1) / BBOX_PER_DEG,
            };
    return bboxes_x * bboxes_y;
}
