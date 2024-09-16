#pragma once

#include <memory>
#include <thread>
#include <nlohmann/json.hpp>

#include "wms.h"

// for a bbox, split it into chunks and for each chunk check if it is stored locally
// if not, add a work queue entry to find it
// returns the number of bboxes in the chunk; how many of them were found locally (as opposed to
// being added to the the work queue) is stored in *local_stored
size_t load_bbox(const struct bbox* outer_bbox, std::unique_ptr<struct bbox[]>* out_bboxes, uint8_t thread_count, size_t* local_stored);

// returns json data for specific chunk that exists locally (errors if not found)
nlohmann::json get_chunk_json_local(const struct bbox* bbox);

// waits for the next update from the server worker to this connection's work queue (thread_counter);
// will spinlock until an update is recieved
nlohmann::json get_chunk_json_workqueue(uint8_t thread_counter);

// only one worker thread can run at once; it takes a const pointer to the total thread count so it
// can dispatch updates to the currently connected thread... practically there is no
// race condition where it fails to send an update to the server, because the work queue mutex means
// that a task with an update to a client will only be fetchable after the client has registered &
// sent an update effecting the work queue -- this means that the total_thread_count will already
// have been updated
void start_worker_thread(const uint8_t* total_thread_count);
void end_worker_thread();
