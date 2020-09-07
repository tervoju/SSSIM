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
#include "env-server.h"
#include "env-gui.h"
#include "env-time.h"
#include "env-clients.h"

const simtime_t gui_sleep_time = 10;


extern simtime_t env_time;

extern T_env_client_vector clients;
extern int clients_awake;
extern pthread_mutex_t clients_mutex;
extern pthread_cond_t client_sleeping;

extern UDPserver * udp;

//-----------------------------------------------------------------------------
void T_env_client::wakeup() {
	//printf(VT_SET(VT_GREEN) "%u: wakeup!\n" VT_RESET, ntohs(addr.sin_port));
	
	if (type == LOG) {
		gui_update(&addr);
		_sleep(env_time + gui_sleep_time);
	} else {
		const time_msg_t msg(MSG_TIME);
		udp_send(&addr, msg.buf, msg.len);
	}
}


//-----------------------------------------------------------------------------
void T_env_client::_sleep(const simtime_t t) {
	//printf(VT_SET(VT_RED) "%u: sleeping for %is\n" VT_RESET, ntohs(addr.sin_port), (int)t - env_time);
	
	const simtime_t prev_t = wakeup_time;
	wakeup_time = t;
	if ((prev_t <= env_time) && (wakeup_time > env_time)) {
		__sync_sub_and_fetch(&clients_awake, 1);
		pthread_cond_signal(&client_sleeping);
	}
}

void T_env_client::sleep(const simtime_t t) {
	pthread_mutex_lock(&clients_mutex);
		_sleep(t);
	pthread_mutex_unlock(&clients_mutex);
}


//-----------------------------------------------------------------------------
void T_env_client::queue_message(size_t len, const char * cdata, simtime_t transit_time) {
	const msg_t msg(env_time + transit_time, len, cdata);

	pthread_mutex_lock(&clients_mutex);
		messages.push_back(msg);
	pthread_mutex_unlock(&clients_mutex);
}


//-----------------------------------------------------------------------------
void T_env_client::handle_messages() {
	if (messages.empty()) return;

	std::list<msg_t>::iterator it = messages.begin();
	while (it != messages.end()) {
		if (it->rx_time <= env_time) {
			udp->sendto(&addr, MSG_SATMSG, it->cdata, it->len);
			it = messages.erase(it);
		} else {
			++it;
		}
	}
}


//-----------------------------------------------------------------------------
T_env_client::T_env_client(const sockaddr_in _addr, const char _type):
	messages(), addr(_addr), type(UNDEFINED), simfloat(NULL), wakeup_time(0)
{
	switch (_type) {
		case FLOAT: case MSG_NEW_FLOAT:	type = FLOAT;     break;
		case BASE:  case MSG_NEW_BASE:	type = BASE;      break;
		case LOG:   case MSG_NEW_LOG:	type = LOG;       break;
		default:                        type = UNDEFINED;
	}
}

T_env_client::T_env_client(const T_env_client & c2):
	messages(c2.messages), addr(c2.addr), type(c2.type), simfloat(c2.simfloat), wakeup_time(c2.wakeup_time)
{ }

T_env_client & T_env_client::operator= (const T_env_client & c2) {
	if (&c2 != this) {
		messages = c2.messages;
		addr = c2.addr;
		type = c2.type;
		simfloat = c2.simfloat;
		wakeup_time = c2.wakeup_time;
	}

	return *this;
}


//-----------------------------------------------------------------------------
int16_t new_client(const sockaddr_in * addr, const char type) {
	pthread_mutex_lock(&clients_mutex);
		int16_t n = clients.size();
		clients.push_back(T_env_client(*addr, type));
	pthread_mutex_unlock(&clients_mutex);
	return n;
}

bool valid_client(int16_t id, T_env_client_type type) {
	if ((id < 0) || (static_cast<size_t>(id) >= clients.size())) return false;
	if ((type != UNDEFINED) && (clients[id].type != type)) return false;
	if ((type == FLOAT) && !clients[id].simfloat) return false;

	return true;
}

int16_t base_client_id() {
	static int16_t id = -1;

	if (id < 0) {
		pthread_mutex_lock(&clients_mutex);
			int16_t s = clients.size();
			for (int16_t i = 0; i < s; ++i)
				if (clients[i].type == BASE) { id = i; break; }
		pthread_mutex_unlock(&clients_mutex);
	}

	return id;
}
