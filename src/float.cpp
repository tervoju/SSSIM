/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <queue>
#include <map>
#include <algorithm>
#include <netinet/in.h>
#include <svl/SVL.h>

class Seafloat;

#include "sssim.h"
#include "udp.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"
#include "util-msgqueue.h"
#include "util-tsdouble.h"
#include "util-trigger.h"
#include "util-leastsquares.h"
#include "util-convert.h"
#include "client-init.h"
#include "client-print.h"
#include "ctd-tracker.h"
#include "ctd-estimate.h"
#include "sat-client.h"
#include "gps-client.h"
#include "dive-ctrl.h"
#include "satmsg-fmt.h"
#include "satmsg-data.h"
#include "satmsg-modem.h"
#include "soundmsg-fmt.h"
#include "soundmsg-modem.h"
#include "float.h"
#include "float-group.h"

// ADD JTE
#include "sea-data.h"
#include "kalman_filter.h"
#include <chrono>
#include <thread>
#include <functional>
#include <semaphore.h>
// END ADD

extern msg_queue satmsg_rx_queue, soundmsg_rx_queue;
extern ts_double bottom_depth;
extern SimSocket *ctrl_socket;

int16_t client_id = -1;
sockaddr_in env_addr;
SimSocket *gui_socket = NULL;
SimTime sim_time;

// ADD JTE
kalman_data Kalman; // for estimating positions with kalman
bool kalmanInitDone = false; // for checking if kalman init is done
Seafloat *sfloat;
//char est_buffer[32768]; // 32kBytes

#define SWARM_NUM_OF_UNITS_IN_GROUP 6 	// in kalman-filter.cpp also
#define kalman_sampling_interval 900  	// 15 min = 900 sec  -> 1 h acoustic comm.
#define kalman_calc_interval 7200		// 2 h must be enough
uint32_t simTimeStamp = 0;
uint32_t simCalcTimeStamp = 0;
sem_t kalman_sem;	// for timers 
// END ADD

//-----------------------------------------------------------------------------
Seafloat::Seafloat(msg_queue *_satmsg_rx_queue, msg_queue *_soundmsg_rx_queue) : env_init_done(false),
																				 init(),
																				 abs_pos(), group(), self_in_group(),
																				 env_depths(),
																				 latest_sat_contact(0),
																				 sat_modem(_satmsg_rx_queue),
																				 snd_modem(),
																				 gps()
{
	group[client_id] = groupfloat();
	self_in_group = &group.begin()->second;
}

//-----------------------------------------------------------------------------
double Seafloat::soundspeed_in_sc() const
{
	const int z_bottom = round(bottom_depth.get());
	int z_sc = -1;
	estimate_soundchannel(z_bottom, &z_sc);
	if (z_sc <= 0)
		return -1.0;
	const double temp = estimate_temperature(z_sc);
	const double salt = estimate_salinity(z_sc);
	const double pressure = pressure_from_depth(z_sc);
	return soundspeed_from_STD(salt, temp, pressure);
}

//-----------------------------------------------------------------------------
bool Seafloat::tx_msg_to_base()
{
	if (!sat_modem.connected)
		return false;
	floatmsg_t msg;
	// SELF
	msg.self = new self_status_t();
	gps_data_t gps_pos;
	const bool gps_ok = gps.get_position(&gps_pos);
	if (gps_ok)
	{
		msg.self->pos = abs_pos_t(gps_pos);
		msg.self->pos_error = 0x00;
	}

	unsigned int i_d = 0;
	FOREACH_REVERSE_CONST(it, env_depths)
	{
		msg.self->prev_depths[i_d] = *it;
		if (++i_d >= DEPTH_HISTORY_SIZE)
			break;
	}
	// ENV
	const int z_bottom = round(bottom_depth.get());
	int z_sc = -1;
	estimate_soundchannel(z_bottom, &z_sc);
	if ((z_bottom > 0) || (z_sc > 0) || !env_depths.empty())
	{
		msg.env = new env_status_t();
		msg.env->z_bottom = z_bottom;
		msg.env->z_sc = z_sc;
		if (env_depths.empty())
			msg.env->n_layers = 0;
		else
		{
			std::sort(env_depths.begin(), env_depths.end());
			msg.env->layers = new env_layer_status_t[env_depths.size()];
			unsigned int n = 0;
			double prev_depth = -100.0;
			FOREACH_CONST(it, env_depths)
			{
				if (*it < prev_depth + 2.0)
					continue;
				env_layer_status_t &l = msg.env->layers[n];
				l.depth = *it;
				l.temp = estimate_temperature(l.depth);
				l.salt = estimate_salinity(l.depth);
				l.flow_x = 0.0;
				l.flow_y = 0.0;
				prev_depth = *it;
				++n;
			}
			msg.env->n_layers = n;
			env_depths.clear();
		}
	}

	const bool tx_ok = sat_modem.tx(-1, &msg);
	if (!tx_ok)
	{
		dprint_fn("tx FAIL\n");
		getchar();
	}
	return tx_ok;
}

//-----------------------------------------------------------------------------
bool Seafloat::rx_msg_from_base(simtime_t t_end)
{
	simtime_t t = sim_time.now();
	int rc = 0;
	size_t msg_counter = 0;

	while ((t < t_end) && (rc >= -2) && sat_modem.connected && sat_modem.unanswered_messages())
	{
		SatMsg sat_msg;
		rc = sat_modem.rx(&sat_msg);

		switch (rc)
		{
		case 1:
			read_basemsg(&sat_msg);
			latest_sat_contact = t;
			++msg_counter;
			//dprint("msg body: %p", &sat_msg.body);
			//for (unsigned int i = 0; i < sat_msg.body_len; ++i) {
			//	if ((i % 8) == 0) printf("\n\t\t");
			//	printf(" %02hhX", sat_msg.body[i]);
			//}
			//putchar('\n');
			break;

		case -2:
			dprint("sat modem: data error\n"); // fallthrough
		case -1:							   // no signal
		case 0:								   // no messages
			t = sim_time.sleep(10);
			break;

		case -3:
			dprint("sat modem: bad alloc\n");
			break;
		default:
			dprint("sat modem: unknown error [%d]\n", rc);
		}
	}
	return (msg_counter > 0);
}

unsigned char Seafloat::read_basemsg(const SatMsg *sat_msg)
{
	switch (sat_msg->type)
	{
	case SATMSG__OK:
		return 0;

	case SATMSG__ERROR_BAD_ID:
		dprint_error("sat rx ERROR: BAD ID\n");
		getchar();
		return 0;

	case SATMSG__ERROR_BAD_DATA:
		dprint_error("sat rx ERROR: BAD DATA\n");
		getchar();
		return 0;
	}

	basemsg_t msg(sat_msg);
	unsigned char msg_fields_remaining = msg.type() ^ SATMSG_BASE_EMPTY;

	if (msg.set_init)
	{
		msg_fields_remaining ^= SATMSG_BASE_GO_INIT;
		memcpy(&init, msg.set_init, sizeof(init));
		env_init_done = false;
		dprint_fn("set_init\n");
	}

	if (msg.set_cycle)
	{
		msg_fields_remaining ^= SATMSG_BASE_GO_CYCLE;
		memcpy(&sim_time.cycle, msg.set_cycle, sizeof(sim_time.cycle));
		dprint_fn("set_cycle: start in %+.1fmin, T %.1fmin; %u floats\n",
				  static_cast<int>(sim_time.cycle.t_start - sim_time.now()) / 60.0,
				  sim_time.cycle.t_period / 60.0,
				  sim_time.cycle.n_floats);
		snd_modem.enabled.set(true);
	}

	if (!msg.group.empty())
	{
		msg_fields_remaining ^= SATMSG_BASE_GROUP;
		dprint_fn("float info gps:");
		FOREACH(it, msg.group)
		{
			int t = it->t;
			double x = it->x, y = it->y;
			group[it->id].add_gps_measurement(it->t, it->x, it->y, this);
			printf(",#%i, %i, %.0f, %.0f", it->id, t, x, y);
		}
		putchar('\n');
		update_gui_predictions();
	}

	// SATMSG_BASE_ENV
	if (msg_fields_remaining)
	{
		char s[32];
		dprint_fn("UNHANDLED msg fields: %s\n", satmsg_type2str(msg_fields_remaining & SATMSG_BASE_EMPTY, s, 32));
	}
	return msg_fields_remaining;
}

//-----------------------------------------------------------------------------
int Seafloat::read_snd_msgs()
{
	int msg_counter = 0;
	int rc = -100;

	while (true)
	{
		SoundMsg msg;
		rc = snd_modem.rx(&msg);
		if (rc <= 0)
			break;
		read_snd_msg(&msg);
		++msg_counter;
	}

	if (rc == 0)
	{
		return msg_counter;
	}
	else
	{
		dprint("snd rx error! (%i)\n", rc);
		return rc;
	}
}

void Seafloat::read_snd_msg(const SoundMsg *msg)
{
	switch (msg->type)
	{
	case SOUNDMSG_PONG:
	{
		const uint16_t *rx_pong = reinterpret_cast<const uint16_t *>(msg->body);
		const size_t n_pong = msg->body_len / sizeof(uint16_t);
		const double ss = 0.001 * soundspeed_in_sc(); // meters per millisecond
		if (ss <= 0.0)
		{
			dprint_fn("ERROR: ss %f \n", ss);
			getchar();
			break;
		}
		dprint("AC ping(m):");
		for (unsigned int i = 0; i < n_pong; ++i)
			if (rx_pong[i])
			{
				const unsigned int dist = round(rx_pong[i] * ss);
				printf(" %u:%u", i, dist);
				distance_float dst(dist, sim_time.now(), i);
				floatDistances.push_back(dst); // push distances to a list
			}
		putchar('\n');
		initkalman();
	}
	break;

	case SOUNDMSG_GPS_POS:
		if (msg->body_len == sizeof(abs_pos_t))
		{
			const abs_pos_t *pos = reinterpret_cast<const abs_pos_t *>(msg->body);
			group[msg->from].add_gps_measurement(pos->t, pos->x, pos->y, this); // add gps measurement to group map
			dprint("AC #%i gps pos (%+.1fmin): %lim N, %lim E\n", msg->from,
				   (static_cast<long int>(pos->t) - sim_time.now()) / 60.0,
				   pos->y, pos->x);
			// real gps fix -> used as is as fix new enough -  that is checked when the message is sent
			pos_xyzt p(pos->x, pos->y, 0.0, pos->t, msg->from);
			floatPositions.push_back(p);
		}
		else
		{
			dprint_fn("AC #%i gps pos ERROR: len %i != %zu\n", msg->from, msg->body_len, sizeof(abs_pos_t));
			getchar();
		}
		break;
	// ADD JTE: float is sending also estimated positions
	case SOUNDMSG_POS_EST:
		if (msg->body_len == sizeof(pos_xyzt))
		{
			const pos_xyzt *pos = reinterpret_cast<const pos_xyzt *>(msg->body);
			dprint("AC:#%i est_pos: (%+.1f min): %.0f, N, %.0f, E, %.2f, Z\n", msg->from,
				   (static_cast<long int>(pos->time) - sim_time.now()) / 60.0,
				   pos->y, pos->x, pos->z);
			// estimate -> only depth is used as z is <> 0.0
			pos_xyzt p(pos->x, pos->y, pos->z, pos->time, pos->floatId);
			floatPositions.push_back(p);
		}
		else
		{
			dprint_fn("AC #%i EST POS ERROR: len %i\n", msg->from, msg->body_len);
			getchar();
		}
		break;
	default:
		dprint("> msg %02hhX [%d] from %d: len %d, time %ds ago\n", msg->type, msg->id, msg->from, msg->body_len, sim_time.now() - msg->time);
	}
}

void Seafloat::updateDistances()
{
	dprint("update distances %d\n", floatDistances.size());
	for (std::list<distance_float>::iterator it = floatDistances.begin(); it != floatDistances.end(); it++) {  
       	distance_float dis = *it;
		updateKalmanDist(client_id, dis.floatId, dis.d);
    }
	floatDistances.clear();
}
void Seafloat::updatePositions()
{
	// update this float information to kalman filter
	gps_data_t gps_pos;
	gps.get_position(&gps_pos, true);
	dprint("update positions %d\n", floatPositions.size());
	if (gps_pos.ok && (gps_pos.t > sim_time.now() - 30 * 60)) { // 30 min!
		const abs_pos_t pos(gps_pos);
		updateKalmanPos(client_id, pos.x, pos.y, 0.0);	// real GPS fix	
	}
	else { // too old fix
		DepthState ds = ctd.get_depth_state();
		updateKalmanPos(client_id, 0.0, 0.0, ds.depth_now); // depth is only used
	}
	// acoustic channel information
	for (std::list<pos_xyzt>::iterator it = floatPositions.begin(); it != floatPositions.end(); it++) {
       pos_xyzt pos = *it;
	   updateKalmanPos(pos.floatId, pos.x, pos.y, pos.z);
    }
	floatPositions.clear();
}

void Seafloat::updateKalmanWithData()
{
	if(kalmanInitDone) {
		updatePositions();
		updateDistances();
	}
}

//-----------------------------------------------------------------------------
void Seafloat::update_gui_predictions()
{
	while (gui_socket == NULL)
	{
		ctrl_socket->send(MSG_GET_GUIADDR);
		usleep(100); // FIXME: HACK
	}
	std::vector<double> pred;
	pos_xyt_conf *my_pos = &self_in_group->gps_pos;
	FOREACH(it, group)
	{
		if (&it->second == self_in_group)
		{
			pred.push_back(my_pos->x); // pos[0]
			pred.push_back(my_pos->y); // pos[1]
		}
		else
		{
			pred.push_back(it->second.gps_pos.x - my_pos->x); // pos[0]
			pred.push_back(it->second.gps_pos.y - my_pos->y); // pos[1]
		}
		pred.push_back(1.0); // err[0]
		pred.push_back(1.0); // err[1]
	}
	gui_socket->send(MSG_FLOAT_STATE, &pred[0], sizeof(double) * pred.size());
}

//
pos_xyzt Seafloat::getKalmanEstimation(int drifter)
{
	pos_xyzt estimate;
	kalman_position_data position_vector[SWARM_NUM_OF_UNITS_IN_GROUP]; // Pointer to array of position_vector elements
	if (kalmanInitDone) {
		read_kalman_positions(&Kalman, position_vector);
		estimate.x = position_vector[drifter - 1].x;
		estimate.y = position_vector[drifter - 1].y;
		estimate.z = position_vector[drifter - 1].z;
		estimate.time = sim_time.now();
		estimate.floatId = drifter;
	}
	else {
		estimate.x = 0.0;
		estimate.y = 0.0;
		estimate.z = 0.0;
	}
	return estimate;
}

void Seafloat::initkalman()
{
	if (kalmanInitDone == false)
	{
		if (group.size() == SWARM_NUM_OF_UNITS_IN_GROUP) // magix number to test with normal
		{
			// Initialization of the Kalman-filter
			initialize_kalman(&Kalman, SWARM_NUM_OF_UNITS_IN_GROUP, kalman_sampling_interval);
			// Set the amount of noise in measurements
			set_measurement_noise(&Kalman, client_id - 1, 10, 0.001, 5, 10000, 0.0001);
			// Set the inaccuracy of the state-space model (treated as white noise)
			set_model_noise(&Kalman, 10, 1, 0.5, 0.0001);
			// Initialize the positions of the units
			set_unit_position(&Kalman, 0, group[1].gps_pos.x, group[1].gps_pos.y, 0.0);
			set_unit_position(&Kalman, 1, group[2].gps_pos.x, group[2].gps_pos.y, 0.0);
			set_unit_position(&Kalman, 2, group[3].gps_pos.x, group[3].gps_pos.y, 0.0);
			set_unit_position(&Kalman, 3, group[4].gps_pos.x, group[4].gps_pos.y, 0.0);
			set_unit_position(&Kalman, 4, group[5].gps_pos.x, group[5].gps_pos.y, 0.0);
			set_unit_position(&Kalman, 5, group[6].gps_pos.x, group[6].gps_pos.y, 0.0);
			// Initialize the sea currents
			// this info is from env. data. to be checked how to to update
			set_current(&Kalman, 0, 0.0, 0.0, 0.0);
			set_current(&Kalman, 1, 0.0, 0.0, 0.0);
			set_current(&Kalman, 2, 0.0, 0.0, 0.0);
			set_current(&Kalman, 3, 0.0, 0.0, 0.0);
			set_current(&Kalman, 4, 0.0, 0.0, 0.0);
			set_current(&Kalman, 5, 0.0, 0.0, 0.0);
			kalmanInitDone = true;
			return;
		}
		else
		{
			return;
		}
	}
	return;
}
void Seafloat::updateKalmanPos(int drifter, double x, double y, double z) 
{
	double value_x = x, value_y = y, value_z = z;
	if (kalmanInitDone) {
		if (value_z == 0.0) {
			position_measurement(&Kalman, drifter-1, 1, value_x, value_y, value_z);
		}
		else {
			position_measurement(&Kalman, drifter-1, 0, value_x, value_y, value_z);
		} 
	}
}

void Seafloat::updateKalmanDist(int drifter, int float_i, double distance) 
{
	if (kalmanInitDone) {
		distance_measurement(&Kalman, drifter-1, float_i-1, distance);
	}
}

void Seafloat::runKalman() 
{
	if (kalmanInitDone) {
		run_kalman(&Kalman);
	}
}

void Seafloat::killKalman() 
{
	if (kalmanInitDone) {
		end_kalman(&Kalman);
	}
}

void doKalman()
{
	uint32_t simtimenow = sim_time.now();
	if ((simtimenow - simTimeStamp) > ((uint32_t)kalman_calc_interval)) {
		if (kalmanInitDone)
		{
			//sem_init(&kalman_sem, 0, 0);
			// input measurement data to Kalman
			//sfloat->updateKalmanWithData();
			// and run kalman filter after this
			run_kalman(&Kalman);

		}
		simTimeStamp = simtimenow;
	}
}

void doKalmanCalc()
{
	uint32_t simtimenow = sim_time.now();
	if ((simtimenow - simCalcTimeStamp) > ((uint32_t)kalman_calc_interval)) {
		if (kalmanInitDone)
		{
			pos_xyzt estimate;
			estimate = sfloat->getKalmanEstimation(client_id); // get right one from kalman filter
			if (estimate.x != 0.0) { // writing to file is lame
				//sprintf(est_buffer, "%d  ,pretalk:  est: %.1fm N, %.1fm E, %.1f, %d, id:%d\n", estimate.time, estimate.y, estimate.x, estimate.z, estimate.time, estimate.floatId);
				dprint("kalman, est: %.1fm N, %.1fm E, %.1f, %d, id:%d\n", estimate.y, estimate.x, estimate.z, estimate.time, estimate.floatId);
			}
			// calc measurement data to Kalman
			sfloat->updateKalmanWithData();
			// and run kalman filter after this
			run_kalman(&Kalman);
		}
		simCalcTimeStamp = simtimenow;
	}
}

void timer_start(std::function<void(void)> func, unsigned int interval)
{
    std::thread([func, interval]() {
        while (true)
        {
            func();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }).detach();
}

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)


//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	 
	
	init_float_client(argc, argv);
	state_t cur_state = STATE_FAIL;
	Seafloat seafloat(&satmsg_rx_queue, &soundmsg_rx_queue);
	seafloat.initkalmandone = false;
	simTimeStamp = sim_time.now();
	simCalcTimeStamp = sim_time.now();
	sfloat = &seafloat;
	timer_start(doKalman, 50);
	timer_start(doKalmanCalc, 100);

	while (true)
	{
		state_t new_state = state_table[cur_state](&seafloat);
		// printf("sim time %d \n", sim_time.now());
		cur_state = new_state;
	}
	/*
	FILE* estFile;
	char filename[80];
	sprintf(filename, "./logs/latest/float_0%d_est",client_id);
	estFile = fopen(filename,"wb");
	if (estFile ){
		printf("file open \n");
        fwrite(est_buffer,1,sizeof(est_buffer),estFile); 
	}
	else {
		printf("file open fails\n");
	}
	fclose(estFile);
	*/
	seafloat.killKalman();
	
	return 0;
}
