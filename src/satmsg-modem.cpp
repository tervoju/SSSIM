/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <errno.h>
#include <netinet/in.h>
#include <queue>
#include <map>
#include <svl/SVL.h>

#include "sssim.h"
#include "sssim-structs.h"
#include "float-structs.h"
#include "client-print.h"
#include "client-time.h"
#include "util-msgqueue.h"
#include "udp.h"
#include "client-socket.h"
#include "sat-client.h"
#include "satmsg-fmt.h"
#include "satmsg-data.h"
#include "satmsg-modem.h"
#include "float-util.h"

extern int16_t client_id;
extern sockaddr_in env_addr;

extern SimTime sim_time;


//-----------------------------------------------------------------------------
SatModem::SatModem(msg_queue * _rx_queue, bool _always_connected, simtime_t _refresh_time, simtime_t _connection_mean_time):
	SatClient(_refresh_time, _connection_mean_time),
	always_connected(_always_connected),
	talk_mutex(),
	rx_queue(_rx_queue),
	comms_log()
{
	pthread_mutex_init(&talk_mutex, NULL);
}

SatModem::~SatModem() {
	pthread_mutex_destroy(&talk_mutex);
}

//-----------------------------------------------------------------------------
void SatModem::update() {
	if (always_connected) return;

	pthread_mutex_lock(&talk_mutex);
		const bool ok = at_surface(&skt);
	pthread_mutex_unlock(&talk_mutex);

	pthread_mutex_lock(&run_mutex);
	pthread_mutex_lock(&connect_mutex);
		connected = (enabled && ok);
		if (connected) { pthread_cond_broadcast(&connect_cond); }
	pthread_mutex_unlock(&connect_mutex);
	pthread_mutex_unlock(&run_mutex);
}


//-----------------------------------------------------------------------------
int SatModem::_tx(const char * cdata, size_t clen) {
	timeval tv = { 1, 0 };
	char reply = -1;
	int r = -1;

	pthread_mutex_lock(&talk_mutex);
		r = skt.UDPclient::send(cdata, clen);
		if (r > 0) r = skt.recv(&reply, 1, &tv);
	pthread_mutex_unlock(&talk_mutex);

	if (r > 0) return reply;
	else {
		dprint("sat tx ERROR: [%d] %s\n", r, strerror(errno));
		return -1;
	}
}

bool SatModem::tx(int16_t to, char type, const void * data, ssize_t data_len) {
	if (!connected && !always_connected) return false;

	if (data == NULL) data_len = 0;
	else if (data_len < 0) data_len = strlen(reinterpret_cast<const char *>(data));

	SatMsg msg(type, data, data_len, to);
	msg.tx_time = sim_time.now();
	msg.msg_id = comms_log[to].next_msg_id;
	msg.ack_id = comms_log[to].next_ack_id;

	const size_t hlen = 1 + sizeof(client_id);
	char * cdata = msg.cdata(hlen);
	const size_t clen = hlen + msg.csize();

	cdata[0] = MSG_SATMSG;
	memcpy(cdata + 1, &client_id, sizeof(client_id));

	bool tx_ok = (_tx(cdata, clen) == SATMSG__OK);
	if (tx_ok) ++comms_log[to].next_msg_id;

	//char type_str[16];
	//dprint("sat tx msg #%u, ack #%u: %s to #%d: %s\n", msg.msg_id, msg.ack_id, satmsg_type2str(msg.type, type_str, 16), msg.to, (tx_ok ? "ok" : "FAIL"));

	return tx_ok;
}


//-----------------------------------------------------------------------------
int SatModem::rx(SatMsg * msg, long timeout_ns) {
	class no_signal { };
	class no_messages { };

	try {
		if (!connected && !always_connected) throw no_signal();

		if (timeout_ns > 0) rx_queue->wait_for_msg(timeout_ns);

		t_msg raw_msg = rx_queue->pop();
		if (raw_msg.empty()) throw no_messages();

		*msg = SatMsg(&raw_msg.front(), raw_msg.size()); 

		const int16_t c_id = (msg->type & SATMSG_BASE_EMPTY) ? -1 : msg->from;
		comms_log[c_id].rx_ack_id = msg->ack_id;
		if (msg->msg_id > comms_log[c_id].next_ack_id) comms_log[c_id].next_ack_id = msg->msg_id;

		//char type_str[16];
		//dprint("sat rx msg #%u, ack #%u: %s from #%d, len %d\n", msg->msg_id, msg->ack_id, satmsg_type2str(msg->type, type_str, 16), msg->from, msg->body_len);

		return 1;
	}
	catch (no_messages x)      { return  0; }
	catch (no_signal x)        { return -1; }
	catch (SatMsg::data_err x) { return -2; }
	catch (std::bad_alloc x)   { return -3; }

	return -4;
}


//-----------------------------------------------------------------------------
int SatModem::unanswered_messages(int16_t id) const {
	if (comms_log.empty()) return 0;

	std::map<int16_t, comms_log_t>::const_iterator it = comms_log.find(id);
	if (it == comms_log.end()) return 0;
	
	return static_cast<int>(it->second.next_msg_id - 1) - static_cast<int>(it->second.rx_ack_id);
}
