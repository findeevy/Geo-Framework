#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <set>
#include <queue>
#include <utility>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>

#include "wms.h"
#include "osm_api.h"
#include "constants.h"

using json = nlohmann::json;
using namespace std;

struct bbox_task {
    struct bbox bbox;
    mutable uint64_t threads;
};


bool compare_bbox(const struct bbox_task a, const struct bbox_task b) {
    return a.bbox.minx > b.bbox.minx || a.bbox.miny > b.bbox.miny;
}


mutex chunk_queue_mutex;
set<struct bbox_task, decltype(compare_bbox)*> chunk_queue(compare_bbox);

atomic_uint8_t run_thread = 0;

struct bbox_loader_queue {
    queue<struct bbox> bboxes_found;
    mutex queue_guard;
};

struct bbox_loader_queue bbox_queues[MAX_CLIENTS];

void upsert_bbox_to_queue(struct bbox_task bbox) {
    chunk_queue_mutex.lock();
    std::pair<set<struct bbox_task>::iterator, bool> pair = chunk_queue.insert(bbox);

    if (pair.second) {
        pair.first->threads |= bbox.threads;
    }

    chunk_queue_mutex.unlock();
}

vector<string> check_bbox_local_file(const struct bbox* bbox) {
    vector<string> fs;
    string fn = get_bbox_filename(bbox);
    // cout << fn << endl;
    for (const filesystem::directory_entry entry
             : filesystem::directory_iterator(GEOJSON_PATH)) {
        if (entry.is_regular_file() && entry.path().stem().generic_string().find(fn) != string::npos) {
            fs.push_back(entry.path().filename());
            // cout << entry.path().filename() << ", ";
        }
    }
    // cout << endl;
    return fs;
}

// -------- exported funcions -----------

size_t load_bbox(const struct bbox* outer_bbox, unique_ptr<struct bbox[]>* out_bboxes, uint8_t thread_count, size_t* local_stored) {
    unique_ptr<struct bbox[]> def_bboxes;
    unique_ptr<struct bbox[]>* bboxes = out_bboxes ? out_bboxes : &def_bboxes;
    size_t nbb = create_normalized_bbox(outer_bbox, bboxes);
    *local_stored = nbb;
    for (int i = 0; i < *local_stored; i++) {
        vector<string> fs = check_bbox_local_file(&(*bboxes)[i]);
        if (fs.empty()) {
            printf("thread %hhu adding bbox %f %f to work queue\n", thread_count, (*bboxes)[i].minx, (*bboxes)[i].miny);
            struct bbox_task bbt = {
                .bbox = (*bboxes)[i],
                .threads = 1ULL << thread_count,
            };
            upsert_bbox_to_queue(bbt);
            // swap unfound bbox to end of list to avoid waiting
            (*bboxes)[i--] = (*bboxes)[--*local_stored];
            (*bboxes)[*local_stored] = bbt.bbox;
        }
    }
    return nbb;
}

json get_chunk_json_local(const struct bbox* bbox) {
    vector<string> fns = check_bbox_local_file(bbox);
    if (fns.empty())
        return NULL;

    json data = {{
            {"minx", bbox->minx},
            {"miny", bbox->miny},
            {"maxx", bbox->maxx},
            {"maxy", bbox->maxy}
        }};// json::array();
    for (const string fn : fns) {
        auto ofn = filesystem::path(GEOJSON_PATH);
        ofn += fn;
        // cout << "opening " << ofn << endl;
        std::ifstream f(ofn);
        data.emplace_back(json::parse(f));
    }
    return data;
}

json get_chunk_json_workqueue(uint8_t thread_counter) {
    printf("fetching workqueue bbox in thread %hhu\n", thread_counter);
    bbox_queues[thread_counter].queue_guard.lock();
    while (bbox_queues[thread_counter].bboxes_found.empty()) {
        bbox_queues[thread_counter].queue_guard.unlock();

        std::this_thread::yield(); // TODO don't spinlock

        bbox_queues[thread_counter].queue_guard.lock();
    }
    struct bbox bb = bbox_queues[thread_counter].bboxes_found.front();
    bbox_queues[thread_counter].bboxes_found.pop();
    bbox_queues[thread_counter].queue_guard.unlock();
    printf("found workqueue bbox in thread %hhu\n", thread_counter);

    return get_chunk_json_local(&bb);
}

void server_bbox_loader_handler(const uint8_t* total_thread_count) {
    while (run_thread) {
        chunk_queue_mutex.lock();
        printf("worker waiting for element\n");
        while (chunk_queue.empty()) {
            chunk_queue_mutex.unlock();
            // TODO don't spinlock
            std::this_thread::yield();
            chunk_queue_mutex.lock();
        }
        auto ext = chunk_queue.extract(chunk_queue.begin());
        struct bbox_task bbt = ext.value();
        chunk_queue_mutex.unlock();
        printf("got element %f %f -- %f %f; notifying threads %016lx\n", bbt.bbox.minx, bbt.bbox.miny, bbt.bbox.maxx, bbt.bbox.maxy, bbt.threads);

        vector<string> fs = check_bbox_local_file(&bbt.bbox);
        if (fs.empty()) {
            printf("worker thread fetching bbox\n");
            fetch_map_for_bounding_box(&bbt.bbox);
        }

        printf("worker thread fetching done!\n");

        for (uint8_t tc = 0, tt = *total_thread_count; tc < tt; tc++) {
            if (bbt.threads & 1ULL << tc) {
                printf("notifying client %hhu\n", tc);
                bbox_queues[tc].queue_guard.lock();
                bbox_queues[tc].bboxes_found.push(bbt.bbox);
                bbox_queues[tc].queue_guard.unlock();
            }
        }
    }
}

thread work_thr;
void start_worker_thread(const uint8_t* total_thread_count) {
    if (!run_thread.fetch_or(1, std::memory_order::relaxed)) {
        work_thr = thread(server_bbox_loader_handler, total_thread_count);
    }
}

void end_worker_thread() {
    run_thread = 0;
    if (work_thr.joinable()) {
        work_thr.join();
    }
}
