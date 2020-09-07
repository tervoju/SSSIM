/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <list>
#include <vector>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "vt100.h"
#include "util-math.h"
#include "util-trigger.h"
#include "util-convert.h"
#include "udp.h"
#include "sssim.h"
#include "sssim-structs.h"
#include "float-structs.h"
#include "satmsg-fmt.h"
#include "satmsg-data.h"
#include "env-float.h"
#include "env-gui.h"
#include "env-time.h"
#include "env-server.h"
#include "env-clients.h"

extern pthread_mutex_t clients_mutex;
extern T_env_client_vector clients;
extern simtime_t env_time;
extern trigger_t runtime;

void show_timerate(bool end);

UDPserver *udp = NULL;

pthread_t udp_thread;

//-----------------------------------------------------------------------------
void udp_send(const sockaddr_in *addr, const void *buf, size_t len)
{
	udp->sendto(addr, buf, len);
}

void udp_broadcast(const void *buf, size_t len)
{
	if (clients.empty())
		return;
	pthread_mutex_lock(&clients_mutex);
	FOREACH_CONST(it, clients)
	udp->sendto(&it->addr, buf, len);
	pthread_mutex_unlock(&clients_mutex);
}

//-----------------------------------------------------------------------------
void ctrl_sleep(int16_t id, size_t csize, const char *cdata)
{
	if (!valid_client(id) || (csize != sizeof(simtime_t)))
		return;

	const simtime_t t = *reinterpret_cast<const simtime_t *>(cdata);
	clients[id].sleep(t);
}

void ctrl_timestate(char state)
{
	runtime.set(state == MSG_PLAY);

	printf("\n\n\t|| Time ");
	switch (state)
	{
	case MSG_PLAY:
		printf("started");
		break;
	case MSG_PAUSE:
		printf("paused");
		break;
	default:
		printf("state: %02hhX", state);
		break;
	}
	printf("\r\e[2A");
	fflush(stdout);
}

//-----------------------------------------------------------------------------
void ctrl_new_client(const sockaddr_in *addr, char type, size_t csize, const char *cdata)
{
	int16_t id_n = new_client(addr, type);
	udp->sendto(addr, &id_n, sizeof(id_n));

	print_timestr();
	printf("%s #%i: %s\n", msg_name(type), id_n, inet(*addr).str);

	if (type == MSG_NEW_FLOAT)
	{
		FloatState fs;
		if (csize == sizeof(fs))
			fs = *reinterpret_cast<const FloatState *>(cdata);

		pthread_mutex_lock(&clients_mutex);
		clients[id_n].simfloat = new SimFloat(id_n, fs);
		print_timestr();
		printf("at %.4f E, %.4f N\n",
			   meters_east_to_degrees(clients[id_n].simfloat->pos[0]),
			   meters_north_to_degrees(clients[id_n].simfloat->pos[1]));
		if (clients[id_n].simfloat->pos[2] != DRIFTER_HALF_HEIGHT)
		{
			print_timestr();
			printf("depth %.2f m\n", clients[id_n].simfloat->pos[2]);
		}
		clients[id_n].wakeup();
		pthread_mutex_unlock(&clients_mutex);
	}

	gui_force_update();
}

//-----------------------------------------------------------------------------
void get_data(const sockaddr_in *addr, int16_t id, char msg_type, size_t csize = 0, const char *cdata = NULL)
{
	if (!valid_client(id, FLOAT))
		return;
	SimFloat *f = clients[id].simfloat;

	size_t len = 0;
	void *buf = NULL;

	switch (msg_type)
	{
	case MSG_GET_CTD:
		buf = f->get_ctd(&len);
		break;
	case MSG_GET_ECHO:
		buf = f->get_echo(&len);
		break;
	case MSG_GET_DENSITY:
		buf = f->get_density(&len);
		break;
	case MSG_GET_GPS:
		buf = f->get_gps(&len);
		break;

	case MSG_GET_INFO:
		buf = f->get_info(&len);
		break;
	case MSG_GET_ENV:
		if ((csize == sizeof(uint32_t)) && (cdata != NULL))
		{
			const uint32_t *depth_i = reinterpret_cast<const uint32_t *>(cdata);
			buf = f->get_env(*depth_i, &len);
		}
		else
			buf = f->get_env(&len);
		break;
		// FIXME: implement
		/*case MSG_GET_DDEPTH:
			if ((csize == sizeof(uint32_t)) && (cdata != NULL)) {
				double density = *reinterpret_cast<uint32_t>(cdata) * 1.0e-3; // transmitted as integer microbars
				buf = f->get_env(depth_i, &len);
			} else buf = f->get_env(&len);
			break;*/

	default:
		printf("|| rx unknown GET: %s (%0hhX) from %s\n", msg_name(msg_type), msg_type, inet(*addr).str);
	}

	if ((buf != NULL) && (len > 0))
		udp->sendto(addr, buf, len);

	free(buf);
}

//-----------------------------------------------------------------------------
void set_data(const sockaddr_in *addr, int16_t id, char msg_type, size_t csize = 0, const char *cdata = NULL)
{
	if (!valid_client(id, FLOAT))
		return;
	SimFloat *f = clients[id].simfloat;

	bool ok = false;
	switch (msg_type)
	{
	case MSG_SET_DENSITY:
		if ((csize == sizeof(double)) && (cdata != NULL))
		{
			const double new_density = *reinterpret_cast<const double *>(cdata);
			ok = f->set_density(new_density);
			//print_timestr(); printf("#%i: set density %.2f kg/m3: %s\n", id, new_density, ok ? "ok" : "FAIL");
		}
		break;

	default:
		printf("|| rx unknown SET: %s (%0hhX) from %s\n", msg_name(msg_type), msg_type, inet(*addr).str);
	}

	udp->sendto(addr, &ok, sizeof(ok));
}

//-----------------------------------------------------------------------------
void handle_satmsg(const sockaddr_in *addr, int16_t id, size_t csize, const char *cdata)
{
	if (!valid_client(id))
		return;

	class bad_id
	{
	};
	class no_signal
	{
	};

	try
	{
		SatMsg msg(cdata, csize);

		if (msg.to < 0)
			msg.to = base_client_id();
		{
			char type_str[32];
			print_timestr();
			printf("msg #%d > #%d: %s [%zdc]: ", id, msg.to, satmsg_type2str(msg.type, type_str, 32), csize);
		}
		if (!valid_client(msg.to))
			throw bad_id();
		if ((clients[id].type == FLOAT) && (!clients[id].simfloat || !clients[id].simfloat->at_surface()))
			throw no_signal();

		udp->sendto(addr, SATMSG__OK);
		printf("ok\n");

		msg.from = id;
		msg.tx_time = env_time;

		clients[msg.to].queue_message(msg.csize(), msg.cdata(), 1);
	}
	catch (SatMsg::data_err x)
	{
		udp->sendto(addr, SATMSG__ERROR_BAD_DATA);
		printf("satmsg: bad data\n");
	}
	catch (bad_id x)
	{
		udp->sendto(addr, SATMSG__ERROR_BAD_ID);
		printf("satmsg: bad rx id\n");
	}
	catch (no_signal x)
	{
		udp->sendto(addr, SATMSG__ERROR_NO_SIGNAL);
		printf("satmsg: no signal\n");
	}
}

//-----------------------------------------------------------------------------
// 0.0 indicates no delivery
double msg_travel_time(const SimFloat *d0, const SimFloat *d1)
{
	if (!d0 || !d1 || (d0 == d1))
		return 0.0;

	double d = len(d1->pos - d0->pos);
	double dz = fabs(d0->pos[2] - d1->pos[2]);

	// model transducer vertical range (±30°); arcsin 0.45 = 26.74°, arcsin 0.55 = 33.37°
	if ((d > 0.0) && !below_threshold(dz / d, 0.5, 0.05))
		return 0.0;

	// model loss wrt distance; separate for in & out of sound channel
	double d_max, d_max_w;
	double ss0, ss1 = -1.0;
	if (d0->in_sound_channel(&ss0) && d1->in_sound_channel(&ss1))
	{
		d_max = 5000.0;
		d_max_w = 2000.0;
	}
	else
	{
		d_max = 300.0;
		d_max_w = 250.0;
	}
	if (!below_threshold(d, d_max, d_max_w) && (drand48() > ACOUSTIC_FREAK_MSG_PROBABILITY))
		return 0.0;

	// model absorption, noise, etc.
	if (drand48() < ACOUSTIC_NOISE_PROBABILITY_AT_MAX_DIST * d / d_max)
		return 0.0;

	// model path of acoustic message
	if (ss1 <= 0.0)
		ss1 = d1->soundspeed();
	double ss = ACOUSTIC_PATH_MODEL((ss0 + ss1) / 2);
	if (ss <= 0.0)
		return 0.0;

	return d / ss;

	/*print_timestr(); printf("%i > %i [%.2fkm]: %.2fs", d0->id, d1->id, d*1.0e-3, d / ss);
	if (d_max > 1000.0) printf(" [ss]\n");
	else putchar('\n');*/
}

//-----------------------------------------------------------------------------
void handle_soundmsg(int16_t id, size_t len, char *buf)
{
	if (!valid_client(id, FLOAT))
		return;
	SimFloat *f = clients[id].simfloat;

	uint32_t *msg_time = reinterpret_cast<uint32_t *>(buf + 1 + sizeof(id));
	const uint32_t tx_time = env_time;

	//print_timestr(); printf("msg [%zuc]", len); for (unsigned int i = 0; i < len; ++i) printf(" %02hhX", buf[i]); putchar('\n');

	pthread_mutex_lock(&clients_mutex);
	FOREACH_CONST(it, clients)
	{
		if (!it->simfloat)
			continue;
		double t = msg_travel_time(f, it->simfloat);
		if (t > 0.0)
		{
			*msg_time = tx_time + (t + 0.5);
			udp->sendto(&it->addr, buf, len);
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void handle_pingpong(int16_t id)
{
	if (!valid_client(id, FLOAT))
		return;
	SimFloat *f = clients[id].simfloat;

	pthread_mutex_lock(&clients_mutex);
	const size_t pong_count = clients.size(); // note: also includes non-float clients, which will only have 0 values
	uint16_t *pong_table = new uint16_t[pong_count];
	unsigned int i = 0;
	FOREACH_CONST(it, clients)
	{
		uint16_t t_ms = 0;
		if (it->type == FLOAT)
		{
			double t1_s, t2_s;
			if ((t1_s = msg_travel_time(f, it->simfloat)) && (t2_s = msg_travel_time(it->simfloat, f)))
			{
				t_ms = 500.0 * (t1_s + t2_s);
			}
		}
		pong_table[i++] = t_ms;
	}
	pthread_mutex_unlock(&clients_mutex);

	//print_timestr(); printf("#%d: ping -> pong:", id);
	//for (unsigned int i = 0; i < pong_count; ++i) printf(" %02hhX", pong_table[i]);
	//putchar('\n');

	udp->sendto(&clients[id].addr, MSG_PINGPONG, pong_table, sizeof(uint16_t) * pong_count);
	delete[] pong_table;
}

//-----------------------------------------------------------------------------
void *udp_server(void *unused)
{
	const unsigned int buf_maxlen = 1500;

	char msg_type;
	int16_t msg_sender;
	size_t msg_len;
	const char *msg_body;
	const ssize_t msg_head_len = sizeof(msg_type) + sizeof(msg_sender);

	char buf[buf_maxlen];
	sockaddr_in addr;

	while (true)
	{
		ssize_t rx_bytes = udp->recvfrom(&addr, buf, buf_maxlen);
		if (rx_bytes <= 0)
			continue;

		msg_type = buf[0];
		if (rx_bytes >= msg_head_len)
		{
			memcpy(&msg_sender, buf + sizeof(msg_type), sizeof(msg_sender));
			msg_len = rx_bytes - msg_head_len;
			msg_body = (msg_len == 0) ? NULL : buf + msg_head_len;
		}
		else
		{
			msg_sender = -1;
			msg_len = 0;
			msg_body = NULL;
		}

		//printf(VT_SET(VT_CYAN) "<< %u %s" VT_SET(VT_DIM) " [%zu]\n" VT_RESET, ntohs(addr.sin_port), msg_name(msg_type), msg_len);

		switch (msg_type)
		{
		case MSG_PLAY:
		case MSG_PAUSE:
			ctrl_timestate(msg_type);
			break;

		case MSG_SLEEP:
			ctrl_sleep(msg_sender, msg_len, msg_body);
			break;

		case MSG_NEW_FLOAT:
		case MSG_NEW_BASE:
		case MSG_NEW_LOG:
			ctrl_new_client(&addr, msg_type, msg_len, msg_body);
			break;

		case MSG_GET_INFO:
		case MSG_GET_ENV:
		case MSG_GET_DDEPTH:
		//case MSG_GET_PROFILE:
		case MSG_GET_CTD:
		case MSG_GET_ECHO:
		case MSG_GET_DENSITY:
		case MSG_GET_GPS:
			get_data(&addr, msg_sender, msg_type, msg_len, msg_body);
			break;

		case MSG_SATMSG:
			handle_satmsg(&addr, msg_sender, msg_len, msg_body);
			break;
		case MSG_SOUNDMSG:
			handle_soundmsg(msg_sender, rx_bytes, buf);
			break;
		case MSG_PINGPONG:
			handle_pingpong(msg_sender);
			break;

		case MSG_GET_GUIADDR:
			udp->sendto(&addr, MSG_GET_GUIADDR | MSG_ACK_MASK, gui_addr(), sizeof(sockaddr_in));
			break;

		case MSG_SET_DENSITY:
			set_data(&addr, msg_sender, msg_type, msg_len, msg_body);
			break;

		case MSG_QUIT:
			runtime.set(false);
			udp_broadcast(MSG_QUIT);

			printf(VT_ERASE_BELOW "\n|| quitting on command.\n");
			show_timerate(true);
			exit(EXIT_SUCCESS);
			break;

		default:
			printf("|| rx %s (%0hhX) from %s\n", msg_name(msg_type), msg_type, inet(addr).str);
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
void udp_init()
{
	udp = new UDPserver(SSS_ENV_ADDRESS);
	pthread_create(&udp_thread, NULL, &udp_server, NULL);
}
