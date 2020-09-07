/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include "udp.h"
#include "client-socket.h"


extern int16_t client_id;


//-----------------------------------------------------------------------------
timeval SimSocket::default_wait_tv = { 0, 3000 };


//-----------------------------------------------------------------------------
ssize_t SimSocket::send(char msg_type, const void * msg_body, size_t body_len, int flags) const {
	const size_t msg_len = 1 + sizeof(client_id) + body_len;
	char * buf = new char[msg_len];
	buf[0] = msg_type;
	void * bp = mempcpy(buf + 1, &client_id, sizeof(client_id));
	if (body_len > 0) memcpy(bp, msg_body, body_len);

	ssize_t tx_bytes = ::send(sockfd, buf, msg_len, flags);

	//printf(VT_SET(VT_MAGENTA) ">> %s" VT_SET(VT_DIM) " [%zu]" VT_RESET, msg_name(msg_type), body_len);
	//if (tx_bytes != (int)msg_len) printf(": " VT_SET2(VT_BRIGHT, VT_RED) "ERROR: tx %zi of %zu bytes\n" VT_RESET, tx_bytes, msg_len);
	//else putchar('\n');

	delete[] buf;
	if (tx_bytes == -1) PRINT_PERROR("send");
	return tx_bytes;
}


//-----------------------------------------------------------------------------
void SimSocket::set_metadata(char type, const timeval wait_tv) {
	data_type = type;
	data_wait_tv = wait_tv;
}

int SimSocket::get_data(void * buf, ssize_t len, char type, const void * request_body, size_t request_body_len) {
	if (!type) type = data_type;
	ssize_t tx_bytes = send(type, request_body, request_body_len);
	if (tx_bytes <= 0) return -1;

	//timeval tv = data_wait_tv;
	return recv(buf, len);
}
