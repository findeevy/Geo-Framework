#pragma once

#include "sockpp/tcp_acceptor.h"

// methods for sending packets over the socket interface

#include "cbor.h"

// the packet should be fully populated and its header field correctly filled before this method
// is called; note that the encoding functions in cbor.cpp are also responsible for correctly
// populating the header, so once the packet is encoded it may be sent without extra work
// returns 0 on success, otherwise returns an error code
int send_packet(sockpp::stream_socket & sock, const struct packet* packet);


// reads a single packet from the server, and populates the packet header with information about
// packet type and length
// returns 0 on success, otherwise an error code
int read_packet(sockpp::stream_socket & sock, struct packet* packet);
