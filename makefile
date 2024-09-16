CC = clang++ -std=c++20 -DNO_GODOT

LINKER_FLAGS = -lsockpp -ltinycbor -lcpr -lgdal

SERVER_DEPS = server.o wms_server/cbor.o wms_server/wms.o wms_server/osm_api.o wms_server/gdal_api.o wms_server/chunk_manager.o wms_server/socket.o

CLIENT_DEPS = client.o wms_server/cbor.o wms_server/wms.o wms_server/socket.o wms_server/godot_bindings.o

all: client server

server: $(SERVER_DEPS)
	$(CC) $(SERVER_DEPS) -o server $(LINKER_FLAGS)
client: $(CLIENT_DEPS)
	$(CC) $(CLIENT_DEPS) -o client $(LINKER_FLAGS)

.cpp.o:
	$(CC) -c $< -o $@

clean:
	rm -rf *~* server client *\#* *.o *.os *.so wms_server/*.o wms_server/*.os godot_project/bin/libwmsclient.*
