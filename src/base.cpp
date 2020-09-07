/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <map>
#include <queue>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "udp.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"
#include "client-init.h"
#include "client-print.h"
#include "util-msgqueue.h"
#include "sat-client.h"
#include "satmsg-fmt.h"
#include "satmsg-data.h"
#include "satmsg-modem.h"

bool read_init_cfg(basemsg_t *msg);  // base-config.cpp
bool read_cycle_cfg(basemsg_t *msg); // base-config.cpp

#define GROUP_GPS_MAX_AGE (60 * 60)
#define MISSION_START_DELAY (30 * 60)

int16_t client_id = -1;
sockaddr_in env_addr;
SimSocket *gui_socket = NULL;
SimTime sim_time;

extern msg_queue satmsg_rx_queue;
SatModem *sat_modem;

void *dive_thread_function(void *unused) { return NULL; } // needed by init_float_client() in client-init.cpp

//-----------------------------------------------------------------------------
struct float_data_t
{
	simtime_t start_time;
	simtime_t env_init_end_time;
	simtime_t latest_comms_time;
	self_status_t status;

	float_data_t() : start_time(0), env_init_end_time(0), latest_comms_time(0), status() {}
};
typedef std::map<int16_t, float_data_t> floats_t;

floats_t floats;

//-----------------------------------------------------------------------------
struct init_t
{
	unsigned int env_dive_ok_count;
	simtime_t env_init_start_time;
	simtime_t mission_start_time;
} init = {0, 0, 0};

//-----------------------------------------------------------------------------
void set_group(basemsg_t *msg, int16_t id)
{
	msg->group.clear();

	const simtime_t t0 = floats[id].latest_comms_time;

	//dprint_fn("\n");
	FOREACH_CONST(it, floats)
	{
		const int16_t &id2 = it->first;
		if ((id2 != id) && (it->second.status.pos.t > 0) && (it->second.latest_comms_time >= t0))
		{
			msg->group.push_back(abs_pos_with_id(id2, it->second.status.pos));
			//dprint("\t#%i: %li %li (%i)\n", msg->group.back().id, msg->group.back().x, msg->group.back().y, msg->group.back().t);
		}
	}
}

//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	init_base_client(argc, argv);
	sat_modem = new SatModem(&satmsg_rx_queue, true);

	dprint("|| waiting for floats...\n");
	while (true)
	{
		//dprint("waiting for messages...\n");
		satmsg_rx_queue.wait_for_msg();

		SatMsg sat_msg;
		const int rc = sat_modem->rx(&sat_msg);
		if (rc != 1)
		{
			switch (rc)
			{
			case 0:
				dprint("sat modem: no messages\n");
				break;
			case -1:
				dprint("sat modem: no signal\n");
				break;
			case -2:
				dprint("sat modem: data error\n");
				break;
			case -3:
				dprint("sat modem: bad alloc\n");
				break;
			default:
				dprint("sat modem: unknown error [%d]\n", rc);
				break;
			}
			continue;
		}

		const simtime_t t_now = sat_msg.tx_time;
		sim_time.set(0, &t_now, sizeof(t_now));

		const int16_t id = sat_msg.from;
		bool is_new_float = false;
		floats_t::iterator f_it = floats.find(id);
		if (f_it == floats.end())
		{
			std::pair<floats_t::iterator, bool> r = floats.insert(std::make_pair(id, float_data_t()));
			f_it = r.first;
			is_new_float = r.second;
		}
		float_data_t &f = f_it->second;

		floatmsg_t rx_msg(&sat_msg);
		unsigned char msg_fields_remaining = rx_msg.type() ^ SATMSG_FLOAT_EMPTY;

		basemsg_t tx_msg;

		if (is_new_float)
		{
			f.start_time = t_now;

			if (!read_init_cfg(&tx_msg))
				exit(EXIT_FAILURE);
			if (!init.env_init_start_time || (t_now > init.env_init_start_time + tx_msg.set_init->t_length - tx_msg.set_init->t_buffer))
			{
				init.env_init_start_time = t_now;
			}
			else
			{
				tx_msg.set_init->t_length -= t_now - init.env_init_start_time;
			}
			tx_msg.set_init->d_start += 0.1 * floats.size();

			dprint("#%i: new float\n", id);
		}

		if (rx_msg.self)
		{
			msg_fields_remaining ^= SATMSG_FLOAT_SELF;

			f.status = *rx_msg.self;

			dprint("#%i: pos (%+ds): ", id, f.status.pos.t - t_now);
			if (f.status.pos_error != 0xFF)
			{
				printf("%dm N, %dm E", f.status.pos.y, f.status.pos.x);
				if (f.status.pos_error == 0)
					printf(" (GPS)\n");
				else
					printf(" (±0x%02hhX)\n", f.status.pos_error);
			}
			else
				printf(" UNKNOWN\n");

			if (f.status.flags)
				dprint("#%i: flags 0x%02hX\n", id, f.status.flags);

			if (f.status.prev_depths[0].data)
			{
				dprint("#%i: depths", id);
				for (unsigned int i = 0; i < DEPTH_HISTORY_SIZE; ++i)
				{
					if (!f.status.prev_depths[i].data)
						break;
					printf(" %i:%u", -(i + 1), f.status.prev_depths[i]);
				}
				putchar('\n');
			}
		}

		if (rx_msg.group)
		{
			msg_fields_remaining ^= SATMSG_FLOAT_GROUP;

			dprint("#%i: unhandled group status (N=%u)\n", id, rx_msg.group->n_floats);
		}

		if (rx_msg.env)
		{
			msg_fields_remaining ^= SATMSG_FLOAT_ENV;

			if ((rx_msg.env->n_layers > 0) && !f.env_init_end_time)
			{
				f.env_init_end_time = t_now;
				++init.env_dive_ok_count;

				if (!init.mission_start_time)
				{
					init.mission_start_time = t_now + MISSION_START_DELAY;
					dprint("|| starting mission in %.2fmin with %d floats\n", MISSION_START_DELAY / 60.0, floats.size());
				}

				read_cycle_cfg(&tx_msg);
				tx_msg.set_cycle->t_start = init.mission_start_time;
				tx_msg.set_cycle->n_floats = floats.size();

				dprint("#%i: env init done\n", id);
			}

			dprint("#%i: env status: SC %um, bottom %um\n", id, rx_msg.env->z_sc, rx_msg.env->z_bottom);
			for (unsigned int i = 0; i < rx_msg.env->n_layers; ++i)
			{
				env_layer_status_t &l = rx_msg.env->layers[i];
				printf("\t\t%3um: % 6.3f°C, % .3f S, (%+.3f, %+.3f) cm/s\n", l.depth, l.temp, l.salt, l.flow_x * 100.0, l.flow_y * 100.0);
			}
		}

		if (msg_fields_remaining)
		{
			char s[32];
			dprint("#%i: unhandled msg fields: %s\n", id, satmsg_type2str(msg_fields_remaining & SATMSG_FLOAT_EMPTY, s, 32));
		}

		if (init.env_dive_ok_count > 1)
			set_group(&tx_msg, id);

		f.latest_comms_time = t_now;

		const bool tx_ok = (tx_msg.type() == SATMSG_BASE_EMPTY)
							   ? sat_modem->tx(id, SATMSG__OK)
							   : sat_modem->tx(id, tx_msg.type(), tx_msg.cdata(), tx_msg.csize());

		if (!tx_ok)
		{
			char s[32];
			dprint("#%i: tx %s: %s (len %zu)\n", id, (tx_ok ? "ok" : "ERROR"), satmsg_type2str(tx_msg.type(), s, 32), tx_msg.csize());
		}
	}

	return 0;
}
