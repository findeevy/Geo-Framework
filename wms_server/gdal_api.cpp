#include <string>
#include <iostream>
#include <format>

#include <gdal.h>
#include <gdal_utils.h>

#include "osm_api.h"
#include "constants.h"

using namespace std;

GDALDatasetH load_osm_to_gdal(string osm_file_loc) {
    int err;

    GDALDatasetH dat = GDALOpenEx(osm_file_loc.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

    if (dat == NULL) {
        cerr << "could not open tmp osm file " << osm_file_loc << endl;
    }
    return dat;
}

int write_osm_to_geojson(string osm_file_name, string out_file_loc) {
    GDALDatasetH dat = load_osm_to_gdal(osm_file_name);
    if (!dat)
        return -1;

    size_t layers = GDALDatasetGetLayerCount(dat);

    for (int i = 0; i < layers; i++) {
        OGRLayerH layer = GDALDatasetGetLayer(dat, i);
        const char* opts_txt[2] = {OGR_L_GetName(layer), NULL};
        GDALVectorTranslateOptions* opts =
            GDALVectorTranslateOptionsNew((char**)opts_txt, NULL);
        string out_file = format(GEOJSON_PATH "{}_{}.geojson", out_file_loc, OGR_L_GetName(layer));
        int err = 0;
        GDALDatasetH out_dat = GDALVectorTranslate(out_file.c_str(), NULL, 1, &dat, opts, &err);
        GDALVectorTranslateOptionsFree(opts);
        if (err) {
            return err;
        }
        if (out_dat) {
            GDALClose(out_dat);
        }

        // for some reason if I try to output multiple
        // layers from the same gdal dataset it will only
        // actually export the first layer with is very
        // odd because it doesn't seem like writing to a
        // file should be a destructive operation on the
        // dataset & i don't see anything in the docs
        // about it being such a one
        GDALClose(dat);
        if (i < layers - 1) {
            dat = load_osm_to_gdal(osm_file_name);
            if (!dat) {
                return -1;
            }
        }
    }
    return 0;
}
