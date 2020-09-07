/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <cstdio>
#include <queue>
#include <map>
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

/* state machine implementation from:
   http://stackoverflow.com/questions/133214/is-there-a-typical-state-machine-implementation-pattern/133361#133361
*/

extern int16_t client_id;
extern SimTime sim_time;

extern ts_double depth_goal;
extern ts_double bottom_depth;

#define DIVE_BUFFER_TIME 20 * 60
#define INIT_SAT_COMMS_TIME 120 * 60
#define SAT_COMMS_TIME 10 * 60
#define SAT_REPLY_WAIT_TIME 3 * 60

void dive_init_wrapper(const init_params_t *conf, std::vector<double> *depths); // from dive-init.cpp

//-----------------------------------------------------------------------------
state_t do_fail(Seafloat *seafloat)
{
	dprint_fn("start\n");
	return STATE_BASE_TALK;
}

//-----------------------------------------------------------------------------
// Modem communication to base station
// ----------------------------------------------------------------------------
state_t do_base_talk(Seafloat *seafloat)
{
	dprint_fn("start\n");

	simtime_t t = sim_time.now();
	const int ct = sim_time.cycle_time(t);
	const simtime_t sat_talk_end_time =
		(ct < -1) ? t + INIT_SAT_COMMS_TIME
				  : (ct < 0) ? sim_time.cycle.t_start - DIVE_BUFFER_TIME
							 : (t - ct + sim_time.cycle.t_period) - DIVE_BUFFER_TIME;

	if (t >= sat_talk_end_time)
	{
		dprint_fn("past sat comm time by %.2fmin, skipping to GROUP TALK\n", (t - sat_talk_end_time) / 60.0);
		return STATE_GROUP_TALK;
	}

	if (seafloat->env_init_done)
		depth_goal.set(0.0);
	else
	{
		dprint_fn("env init not done, surfacing...\n");
		go_to_surface(1000.0);
		dprint_fn("at surface.\n");
	}

	seafloat->sat_modem.start();
	seafloat->gps.start();
	dprint_fn("sat clients started, comms end in %.2fmin\n", (sat_talk_end_time - static_cast<double>(t)) / 60.0);

	const simtime_t gps_wait_time = sat_talk_end_time - SAT_REPLY_WAIT_TIME;
	dprint_fn("gps timout in %.2fmin\n", (gps_wait_time - static_cast<double>(t)) / 60.0);

	// TODO? spawn two threads here, one to wait_for_connection, the other to act as timeout
	while (t < sat_talk_end_time)
	{
		if (seafloat->sat_modem.connected && ((t > gps_wait_time) || seafloat->gps.connected))
			break;
		else
			t = sim_time.sleep(10);
	}

	dprint_fn("gps: ");
	gps_data_t gps_pos;
	const bool gps_ok = seafloat->gps.get_position(&gps_pos);
	const uint32_t first_gps_time = gps_ok ? gps_pos.t : 0;
	if (gps_ok)
	{
		// ADD JTE
		//pos_xyzt estimate = seafloat->getKalmanEstimation(client_id); // get first estimate
		seafloat->self_in_group->add_gps_measurement(gps_pos.t, gps_pos.x, gps_pos.y, seafloat);
		printf("%.1fm N, %.1fm E\n", gps_pos.y, gps_pos.x); 
		// ADD JTE
		//if (estimate.x != 0.0) { // if there is no estimate don't print it
		//	dprint_fn("est: ");
		//	printf("%.1fm N, %.1fm E, %.1f, %d, id:%d\n", estimate.y, estimate.x, estimate.z, estimate.time, estimate.floatId);
		//}
	}
	else
	{
		printf("NO CONNECTION\n");
		getchar();
	}

	const bool tx_ok = seafloat->tx_msg_to_base();
	dprint_fn("sat tx %s\n", tx_ok ? "ok" : "FAIL");

	const bool rx_ok = seafloat->rx_msg_from_base(sat_talk_end_time);
	dprint_fn("sat rx %s\n", rx_ok ? "ok" : "FAIL");

	if (seafloat->gps.get_position(&gps_pos) && (gps_pos.t > first_gps_time))
	{
		seafloat->self_in_group->add_gps_measurement(gps_pos.t, gps_pos.x, gps_pos.y, seafloat);
		dprint_fn("gps: %.1fm N, %.1fm E\n", gps_pos.y, gps_pos.x);
	}

	seafloat->sat_modem.stop();
	seafloat->gps.stop();
	dprint_fn("sat clients stopped\n");
	// tx and rx ok -> and env init done
	return (!tx_ok || !rx_ok) ? STATE_FAIL
							  : seafloat->env_init_done ? STATE_GROUP_TALK
														: seafloat->init.ok() ? STATE_INIT_DIVE
																			  : STATE_FAIL;
}

//-----------------------------------------------------------------------------
state_t do_init_dive(Seafloat *seafloat)
{
	dprint_fn("start\n");
	dive_init_wrapper(&seafloat->init, &seafloat->env_depths);
	seafloat->env_init_done = true;
	return STATE_BASE_TALK;
}

//-----------------------------------------------------------------------------
void group_talk_pre_comms(Seafloat *seafloat)
{
	const int cn = sim_time.cycle_number();
	putchar('\n');
	dprint("## cycle %i\n", cn);
	//
	seafloat->floatDistances.clear(); // empty distance list
	seafloat->floatPositions.clear();
	seafloat->read_snd_msgs();
	// send gps data
	gps_data_t gps_pos;
	seafloat->gps.get_position(&gps_pos, true);
	if (gps_pos.ok && (gps_pos.t > sim_time.now() - 120 * 60))
	{
		const abs_pos_t pos(gps_pos);
		seafloat->snd_modem.tx(SOUNDMSG_GPS_POS, &pos, sizeof(pos));
	}
	// ADD JTE
	// this should estimate position with possible pings, and/or previous positions
	// send only if there is no recent gps fix available
	else {
		pos_xyzt estimate;
		estimate = seafloat->getKalmanEstimation(client_id); // get right one from kalman filter
		if (estimate.x != 0.0) {
			printf("%d  ,pretalk est %.1fm N, %.1fm E, %.1f, %d, id:%d\n",estimate.time, estimate.y, estimate.x, estimate.z, estimate.time, estimate.floatId);
			// what is send to others is estimate x,y and actual depth
			DepthState ds = ctd.get_depth_state();
			const pos_xyzt est_pos(estimate.x, estimate.y, ds.depth_now, estimate.time, estimate.floatId);
			seafloat->snd_modem.tx(SOUNDMSG_POS_EST, &est_pos, sizeof(est_pos));
		}
	}
	// set new depth using 6-cycle
	const int loop_count = (cn + client_id) % 6;
	const bool talk_to_base = (loop_count == 0);
	const unsigned int z = 8.0 + drand48() * 72.0;
	depthcode dc(z, talk_to_base, false);
	seafloat->self_in_group->set_depthcode(cn, dc);
}

void group_talk_post_comms(Seafloat *seafloat)
{
	seafloat->read_snd_msgs();
	seafloat->update_gui_predictions();
}

state_t do_group_talk(Seafloat *seafloat)
{
	dprint_fn("start\n");
	int ct = sim_time.cycle_time(0, true);
	if (ct < 0)
	{
		dprint_fn("ct %i -> FAIL\n", ct);
		getchar();
		return STATE_FAIL;
	}

	const int ct_wait = sim_time.cycle.t_period - ct;
	const int ct_talk_end = sim_time.cycle.n_floats * sim_time.cycle.tx_slot_seconds;

	if ((ct > ct_talk_end) && (ct_wait > 3 * DIVE_BUFFER_TIME))
	{
		dprint_fn("here too early (%.2fmin to comms), let's go drift instead\n", ct_wait / 60.0);
		return STATE_DRIFT;
	}

	const int z_bottom = round(bottom_depth.get());
	int z_sc = -1;
	estimate_soundchannel(z_bottom, &z_sc);
	dprint_fn("soundchannel at %dm, diving...\n", z_sc);
	depth_goal.set(z_sc);
	const int ct_tx_slot = (client_id - 1) * sim_time.cycle.tx_slot_seconds; // base should/will have id #0; FIXME: verify

	if (ct > ct_talk_end)
	{
		//dprint_fn("sleeping until tx slot: %i + %is...\n", ct_wait, ct_tx_slot);
		sim_time.sleep(ct_wait + ct_tx_slot);
	}

	// TIME NOW: before own comms tx slot
	group_talk_pre_comms(seafloat);

	ct = sim_time.cycle_time(0, true);
	if (ct < 0)
	{
		dprint_fn("ct %i -> FAIL\n", ct);
		getchar();
		return STATE_FAIL;
	}
	if (ct < ct_talk_end)
	{
		//dprint_fn("sleeping until end of comms: %is...\n", ct_talk_end - ct);
		sim_time.sleep(ct_talk_end - ct);
	}

	// TIME NOW: at end of all comms
	group_talk_post_comms(seafloat);

	DepthState ds = ctd.get_depth_state();
	dprint("depth: %.2fm (vel %+.2fcm/s, var %.5f, T %.0fs)\n", ds.depth_now, ds.slope * 100, ds.variance, ds.time_valid);
	return STATE_DRIFT;
}

//-----------------------------------------------------------------------------
state_t do_drift(Seafloat *seafloat)
{

	//dprint_fn("start\n");
	const int cn = sim_time.cycle_number();
	depthcode dc = seafloat->self_in_group->get_depthcode(cn);
	if (dc.is_unknown())
	{
		dprint_fn("dc unknown -> FAIL\n");
		return STATE_FAIL;
	}

	const unsigned int z = dc.depth();
	depth_goal.set(z);
	seafloat->env_depths.push_back(z);

	const simtime_t t = sim_time.now();
	const int ct = sim_time.cycle_time(t, true);
	if (ct < 0)
	{
		dprint_fn("ct %i -> FAIL\n", ct);
		return STATE_FAIL;
	}

	int ct_drift_end = sim_time.cycle.t_period - DIVE_BUFFER_TIME;
	if (dc.surfacing())
		ct_drift_end -= SAT_COMMS_TIME + DIVE_BUFFER_TIME;

	dprint_fn("idle at %um for %.1fmin, then -> %s\n", z, (ct_drift_end - ct) / 60.0, dc.surfacing() ? "BASE TALK" : "GROUP TALK");

	if (ct < ct_drift_end)
		sim_time.sleep(ct_drift_end - ct);
	return dc.surfacing() ? STATE_BASE_TALK : STATE_GROUP_TALK;
}

//-----------------------------------------------------------------------------
state_t do_profile(Seafloat *seafloat)
{
	dprint_fn("start\n");
	return STATE_BASE_TALK;
}
