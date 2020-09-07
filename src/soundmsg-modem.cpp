/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h> // for putchar
#include <cstring>
#include <queue>
#include <map>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "udp.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"
#include "util-msgqueue.h"
#include "util-trigger.h"
#include "client-print.h"
#include "sat-client.h"
#include "gps-client.h"
#include "satmsg-fmt.h"
#include "satmsg-data.h"
#include "satmsg-modem.h"
#include "soundmsg-fmt.h"
#include "soundmsg-modem.h"
#include "float.h"

extern int16_t client_id;
extern sockaddr_in env_addr;
extern SimTime sim_time;
extern msg_queue soundmsg_rx_queue;

//-----------------------------------------------------------------------------
SoundModem::SoundModem() : thread_id(),
						   next_msg_id(1),
						   tx_queue(),
						   enabled(false)
{
	pthread_create(&thread_id, NULL, &thread_wrap, this);
}

//-----------------------------------------------------------------------------
void *SoundModem::thread()
{
	SimSocket tx_skt(&env_addr);

	while (true)
	{
		if (!enabled.get())
			sim_time.suspend_sleep();
		enabled.wait_for(true);

		const int ct = sim_time.cycle_time(0, true);

		if (!sim_time.cycle.tx_slot_seconds || (ct < 0) || (client_id < 0))
		{
			enabled.set(false);
			continue;
		}

		sim_time.enable_sleep();

		const int ct_tx_slot_start = (client_id - 1) * sim_time.cycle.tx_slot_seconds; // base should/will have id #0

		int sleep_time = 0;

		if (ct < ct_tx_slot_start)
		{
			sleep_time = ct_tx_slot_start - ct;
		}
		else if (ct < ct_tx_slot_start + SOUNDMODEM_PING_GAP)
		{
			tx_skt.send(MSG_PINGPONG);
			//dprint(" > ping\n");
			sleep_time = SOUNDMODEM_PING_GAP;
		}
		else if (ct < ct_tx_slot_start + sim_time.cycle.tx_slot_seconds)
		{
			t_msg tx_msg = tx_queue.pop();
			if (!tx_msg.empty())
			{
				tx_skt.UDPclient::send(&tx_msg.front(), tx_msg.size());
				//dprint(" > msg %zuc\n", tx_msg.size());
			}
			sleep_time = SOUNDMODEM_TX_MSG_GAP;
		}
		else
		{
			sleep_time = sim_time.cycle.t_period - ct;
		}

		sim_time.sleep(sleep_time);
	}

	return NULL;
}

//-----------------------------------------------------------------------------
bool SoundModem::tx(SoundMsg &msg)
{
	msg.from = client_id;

	const size_t hlen = 1; // expected msg_sender at cdata[1] set by msg.from
	char *cdata = msg.cdata(hlen);
	const size_t clen = hlen + msg.csize();

	cdata[0] = MSG_SOUNDMSG;

	//dprint("sound tx: msg %02hhX id #%d, len %d\n", msg.type, msg.id, data_len);
	if (tx_queue.push(cdata, clen))
	{
		++next_msg_id;
		//dprint("tx"); for (unsigned int i = 0; i < clen; ++i) printf(" %02hhX", cdata[i]); putchar('\n');
		//printf("\t:: from %i time %u type %02hhX id %02hhX body_len %u\n", msg.from, msg.time, msg.type, msg.id, msg.body_len);
		return true;
	}
	else
		return false;
}

bool SoundModem::tx(char type, const void *data, ssize_t data_len)
{
	if (data == NULL)
		data_len = 0;
	else if (data_len < 0)
		data_len = strlen(reinterpret_cast<const char *>(data));

	SoundMsg msg(type, data, data_len, next_msg_id);
	return tx(msg);
}

//-----------------------------------------------------------------------------
int SoundModem::rx(SoundMsg *msg)
{
	class no_messages
	{
	};

	try
	{
		t_msg raw_msg = soundmsg_rx_queue.pop();
		if (raw_msg.empty())
			throw no_messages();

		*msg = SoundMsg(&raw_msg.front(), raw_msg.size());
		//dprint("rx"); for (unsigned int i = 0; i < raw_msg.size(); ++i) printf(" %02hhX", raw_msg[i]); putchar('\n');
		//printf("\t:: from %i time %u type %02hhX id %02hhX body_len %u\n", msg->from, msg->time, msg->type, msg->id, msg->body_len);
	}
	catch (no_messages x)
	{
		return 0;
	}
	catch (SoundMsg::data_err x)
	{
		return -2;
	}
	catch (std::bad_alloc x)
	{
		return -3;
	}

	return 1;

	//dprint("sat rx: ");
	//if (r != 0) dprint("sat rx: [%d] %s\n", r, strerror(r));
	//else printf("msg %02hhX from #%d, len %zd\n", msg->head.type, msg->head.from, msg->body.size());
}
