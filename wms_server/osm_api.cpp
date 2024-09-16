#include <iostream>
#include <fstream>
#include <string>
#include <format>
#include <cpr/cpr.h>

#include "osm_api.h"
#include "gdal_api.h"
#include "constants.h"

using namespace std;

//#define DO_NOT_QUERY_WEB

mutex osm_tmp_file_mutex;

int fetch_map_for_bounding_box(const struct bbox* query) {
    string bbox = std::format("{},{},{},{}", query->minx, query->miny, query->maxx, query->maxy);
#ifndef DO_NOT_QUERY_WEB
    cpr::Response r = cpr::Get(cpr::Url{OSM_API_URL}, cpr::Parameters{{"bbox", bbox}});

    cout << r.status_code << "\t" << r.header["content-type"] // << "\n" << r.text
         << endl;

    if (r.status_code != 200)
        return r.status_code;
#endif
    lock_guard<mutex> guard(osm_tmp_file_mutex);
#ifndef DO_NOT_QUERY_WEB
    ofstream outfile; // TODO ? it would be nicer if we could use virtual memeory here instead of creating a temp file but that would require more effort
    outfile.open(TMP_OSM_FILE, ios::trunc);
    outfile << r.text;
    outfile.close();
#endif

    return write_osm_to_geojson(TMP_OSM_FILE, get_bbox_filename(query));
}

void fetch_bounding_box_for_city(string city_name, struct bbox* query) {
    return; // TODO ?
}
