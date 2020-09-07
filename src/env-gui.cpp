/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <list>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "sssim-structs.h"
#include "util-convert.h" 
#include "udp.h"
#include "env-float.h"
#include "env-time.h"
#include "env-clients.h"

extern pthread_mutex_t clients_mutex;
extern T_env_client_vector clients;

extern UDPserver * udp;


//-----------------------------------------------------------------------------
sockaddr_in * gui_addr() {
	static sockaddr_in addr;
	if (addr.sin_port > 0) return &addr;

	bool got_gui = false;
	pthread_mutex_lock(&clients_mutex);
		FOREACH_CONST(it, clients) {
			if (it->type == LOG) { 
				addr = it->addr; 
				got_gui = true;
				break; 
			}
		}
	pthread_mutex_unlock(&clients_mutex);

	return got_gui ? &addr : NULL;
}


//-----------------------------------------------------------------------------
void gui_force_update() {
	pthread_mutex_lock(&clients_mutex);
		FOREACH(it, clients) {
			if (it->type == LOG) it->wakeup_time = 0;
		}
	pthread_mutex_unlock(&clients_mutex);
}


//-----------------------------------------------------------------------------
void gui_update(sockaddr_in * addr) {
	std::vector<FloatState> floats;
	floats.reserve(clients.size());

	floats.push_back(FloatState()); // blank; space to use for header

	// clients_mutex should've been locked by the caller
	FOREACH_CONST(it, clients) if (it->simfloat) {
		FloatState * f = reinterpret_cast<FloatState *>(it->simfloat->get_info(NULL));
		floats.push_back(*f);
		free(f);
	}

	time_msg_t header(MSG_ENV_STATE);

	// hackyness to save a memcpy() of floats to buf
	char * buf = reinterpret_cast<char *>(&floats[1]);
	buf -= header.len;
	memcpy(buf, header.buf, header.len);

	const size_t floats_bytesize = (floats.size() - 1) * sizeof(FloatState);
	udp->sendto(addr, buf, header.len + floats_bytesize);
}
