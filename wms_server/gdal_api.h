#pragma once

#include <gdal.h>

// converts an osm file into geojson format files (each osm file will result in multiple
// geojson files, because each feature must be stored seperately)

GDALDatasetH load_osm_to_gdal(std::string osm_file_loc);

int write_osm_to_geojson(std::string osm_file_loc, std::string out_file_loc);
