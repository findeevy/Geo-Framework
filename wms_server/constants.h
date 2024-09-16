// utility constants for configuring server behaviour

#define SERVER_PORT sockpp::TEST_PORT

// where geodata files are stored, and where server checks to see if files exist locally
#define GEOJSON_PATH "./wms_server/geodata/"

// url up to the GET request ? of the osm api
#define OSM_API_URL "https://api.openstreetmap.org/api/0.6/map"

// where server stores osm info before converting to geojson
#define TMP_OSM_FILE "./wms_server/tmp.osm"

// #define OVERPASS_API_URL "https://overpass-api.de/api/interpreter"

// how many bounding boxes we divide each degree of lattitiude into
// we want this to be an integer because it makes the math much easier
// & removes the possibility of floating point innacuracy rounding errors
#define BBOX_PER_DEG_INT (100)

#define BBOX_PER_DEG ((float)BBOX_PER_DEG_INT)

// we have a limit of 64 clients because we use a uint64_t as a bitvector
// to encode which clients requested a particular bounding box
// this could be changed to a uint64_t[...] to increase the allowed number
#define MAX_CLIENTS 64
