#include <iostream>
#include <string>

#include "wms_server/godot_bindings.h"

using namespace std;
using namespace godot;

// this is a testing sub to run a GDClient outside of godot...
// the godot_bindings file should be compiled with -DNO_GODOT to mock the godot signals
// and treat godot strings as normal c++ strings etc...

int main() {
    string host = "localhost";
    in_port_t port = sockpp::TEST_PORT;

    GDClient* client = new GDClient();

    int result = client->connect_to_server(host, port);

    if (result != 0) {
        printf("couldn't connect: res: %d\n", result);
    } else {

        float minx = 11.54, miny = 48.14, maxx = 11.543, maxy = 48.145;

        while (1) {

#ifdef TEST_GET_BBOX_CIN
            cin >> minx >> miny >> maxx >> maxy;
#endif
            client->queue_fetch_bbox(minx, miny, maxx, maxy);

            break;
        }

        string s;
        printf("press enter to quit:...\n");
        getline(cin, s);
        printf("quitting...\n");
    }

    client->disconnect();

    delete client;
    return 0;
}
