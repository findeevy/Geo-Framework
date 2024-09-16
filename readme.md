# Godot geoframework

This project contains a C++ server for managing and distributing geospatial data, and a Godot frontend which connects to the server to recieve geospatial data.

## C++ Dependencies:

The following can be built from source or installed with the package manager of your choice:

[sockpp](https://github.com/fpagliughi/sockpp) wrapper for socket i/o

[tinycbor](https://github.com/intel/tinycbor) binary encoding format used in packet transmission

[cpr](https://github.com/libcpr/cpr) wraper for cURL, used to fetch data from the OSM api

[gdal](https://github.com/OSGeo/GDAL) tools for managing geospatial data, used to convert osm data into geojson

[json](https://github.com/nlohmann/json) used to parse and transmit geojson files

Note that if only the client needs to be run on a machine, only `sockpp`, `tinycbor` and `json` need be installed; the other dependencies are used only by the server.

The `godot_project/bin/gdwmsclient.gdextension` should be modified to include the client dependencies when building to a system other than linux; linux dynamic libraries are already provided.

## Running the server

Once the dependencies have been installed, the server can be built by running

```
make server
```

And run with

```
./server
```

The server is currently run on port `12345`; this can be changed in `wms_server/constants.h`.

To test the server locally, without issuing new fetch requests to the OSM api, build it with `-DDO_NOT_QUERY_WEB`; this will copy an existing `tmp.osm` into new geojson files rather than downloading the correct osm data for each bounding box. Note that the api needs to be queried at least once to have a tmp file to use.

## Building Godot with the Client extension

See [GDExtension C++ Example](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_cpp_example.html) for full details of the process.

You will need to have [Scons](https://scons.org/) installed to build godot-cpp

Initialize the godot-cpp submodule and build it, replacing `<platform>` with `linux`, or the non-free systems `macos` or `windows`:

```
git submodule update --init
cd godot-cpp
scons platform=<platform>
```
NOTE: godot's scons build scripts are buggy; if scons gives an error that `BoolVariable` is not defined, add `from SCons.Script import *` to the top of the affected files (`godot-cpp/tools/ios.py`, `godot-cpp/tools/linux.py`, `godot-cpp/tools/windows.py`, `godot-cpp/tools/godotcpp.py`) and then it should build.


Then, `cd` to the project's root directory run

```
scons platform=<platform>
```

again to build our godot extension.

When the project is run (`godot godot_project/project.godot`), a `GDClient` object can call the public class funcitons specified in `wms_server/godot_bindings.h`.

Note that the server should be running when the godot project is run, or the client will obviously be unable to connect.


### Godot-free test client

To run a client without godot, or additional clients, run

```
make client
./client
```

this client will call the same methods in `wms_server/godot_bindings.h`, but mocks the godot signals & types, so can be used for testing just the client / server communication.

## Godot Front-End

The movement_controller.gd script is a modified of [Luciusponto's Player Controller](https://github.com/luciusponto/godot_first_person_controller).

Large amounts of surfaces being rendered may exceed Godot's "MAX_MESHES_SURFACE" which requires modification of the project files to increase. Go to project settings then rendering/limits/opengl/max_renderable_elements.

Currently the front end supports the following GeoJSON features:
<p>-Points</p>
<p>-LineStrings</p>
<p>-MultiLineStrings</p>
<p>-Polygons</p>
<p>-Multipolygons</p>

## Project Organization

![data transfer diagram](/data_transfer_diagram.png)

All the files for the godot front-end for the project are in `godot_project/`.

The main method for the server is in `server.cpp`; source code for additional functionality used by the server is in `wms_server/`.

The bindings for to communicate with the server are in `wms_server/godot_bindings.h` / `.cpp`; these same bindings are used by `client.cpp` to mock godot.

`godot-cpp` contains the godot's c++ source code, and must be compiled to build an extension for godot, but should not otherwise be modified.

By default, geojson files created by the server will be stored in `wms_server/geodata/`; this, along with other general server configuration settings, can be modified in `wms_server/constants.h`.
