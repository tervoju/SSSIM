/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <cstdio>
#include <vector>
#include <netinet/in.h>
#include <svl/SVL.h>
#include <deque>

#include "vt100.h"
#include "util-leastsquares.h"
#include "util-convert.h"
#include "util-trigger.h"
#include "util-tsdouble.h"
#include "sssim.h"
#include "udp.h"
#include "sssim-structs.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"
#include "client-print.h"
#include "ctd-tracker.h"
#include "ctd-estimate.h"
#include "float-util.h"
#include "dive-ctrl.h"

#ifdef DT_DEBUG
#undef dprint
#undef printf
#define dprint(...)
#define printf(...)
#endif

extern int16_t client_id;
extern sockaddr_in env_addr;
extern SimTime sim_time;

// set elsewhere, read here
trigger_t enable_dive_ctrl(false);
ts_double depth_goal(0.0);
ts_double depth_tolerance(1.0);
ts_double bottom_min_distance(3.0);
ts_double bottom_near_distance(6.0);

// set here, read elsewhere
ts_double bottom_depth(255.0);

double bottom_depth_adjustment = 0.0;

//-----------------------------------------------------------------------------
enum dive_state_t
{
	adjusting_depth,
	stable_depth
} dive_state = stable_depth;
static int ctd_wait_time[] = {3, 30}; // adjusting, stable

bool dive_is_stable() { return dive_state == stable_depth; }

//-----------------------------------------------------------------------------
void print_ds(const DepthState &ds)
{
	printf("%.1fm %+.2fcm/s, v %.2f @ %.0f s", ds.depth_now, ds.slope * 100, ds.variance, ds.time_valid);
}

//-----------------------------------------------------------------------------
double go_to_surface(double d_surface)
{
	if (d_surface <= 0.0)
		d_surface = 1000.0;

	double d_now;
	do
	{
		d_now = get_float_density();
		if (d_now <= -3.0)
			return d_now;
	} while (d_now <= 0.0);
	if (d_surface > d_now)
		d_surface = d_now;

	enable_dive_ctrl.set(false);
	depth_goal.set(0.0);

	// start dive
	dprint_dim("%s: start from %.2f kg/m³, going to %.2f kg/m³\n", __FUNCTION__, d_now, d_surface);
	ctd.reset();
	if ((d_now <= 0.0) || (d_surface < d_now))
		while (!set_float_density(d_surface))
		{
			dprint_error("%s: error setting density -> retrying...\n", __FUNCTION__);
			sim_time.sleep(10);
		}
	double d_set = d_surface;

	simtime_t piston_stop_time = 0, print_time = 0;
	double piston_stop_depth = -1.0;

	bool at_surface = false;
	do
	{
		const bool piston_stable = (d_now > 0.0) && (fabs(d_now - d_set) < 1e-3);
		if (!piston_stable)
			piston_stop_time = 0;

		ctd_data_t ctd_now;
		do
		{
			//dprint("%s: reading ctd\n", __FUNCTION__);
			sim_time.sleep(2); // FIXME: debug to model CTD delay
			ctd_now = get_ctd();
		} while (ctd_now.pressure == 0);
		//dprint("%s: CTD %.1f mS/cm, %.1f°C, %u mbar -> %.1f m\n", __FUNCTION__,
		//	ctd_now.conductivity, ctd_now.temperature, ctd_now.pressure,
		//	depth_from_pressure(ctd_now.pressure - STANDARD_ATMOSPHERIC_PRESSURE));

		ctd.add(ctd_now);
		DepthState ds = ctd.get_depth_state();
		if (ds.depth_now == 0.0)
		{
			if (ctd_now.pressure > STANDARD_ATMOSPHERIC_PRESSURE)
				ds.depth_now = depth_from_pressure(ctd_now.pressure - STANDARD_ATMOSPHERIC_PRESSURE);
			ds.slope = 0.0;
			ds.time_valid = 0;
		}

		if (!print_time || (print_time + 30 <= ctd_now.time))
		{
			dprint_dim("%s: %.1f/%.1fm %+.1fcm/s (%.0fs)\n", __FUNCTION__,
					   depth_from_pressure(ctd_now.pressure - STANDARD_ATMOSPHERIC_PRESSURE),
					   ds.depth_now, 100 * ds.slope, ds.time_valid);
			print_time = ctd_now.time;
		}

		if ((piston_stop_time == 0) && piston_stable)
		{
			piston_stop_time = ctd_now.time;
			piston_stop_depth = ds.depth_now;
		}

		if (piston_stable)
		{
			at_surface = (ds.slope < 0.001) && (ds.depth_now <= DRIFTER_SURFACE_DEPTH);
			const bool time_to_check_density = (ds.slope > 0.0) || (((ds.slope != 0.0) && (ds.slope >= -0.01)) && (ctd_now.time > piston_stop_time + 60)) || (ctd_now.time > piston_stop_time + 120 * piston_stop_depth);
			if (!at_surface && time_to_check_density)
			{
				d_set -= 0.2;
				dprint_dim("%s: adjusting goal to %.2f kg/m³\n", __FUNCTION__, d_set);
				ctd.reset();
				while (!set_float_density(d_set))
				{
					sim_time.sleep(10);
				}
			}
		}
		else
		{
			at_surface = false;
			//dprint("%s: piston moving: %.2lf -> %.2lf kg/m³\n", __FUNCTION__, d_now, d_set);
			d_now = get_float_density();
		}

		sim_time.sleep(1);
	} while (!at_surface);

	dprint_dim("%s: done, at %.2f kg/m³\n", __FUNCTION__, d_set);

	enable_dive_ctrl.set(true);

	return d_set;
}

//-----------------------------------------------------------------------------
void *dive_thread_function(void *unused)
{
	double prev_z_goal = -1.0;
	bool near_z_goal = false;
	double density_set = -1.0;
	uint32_t surface_escape_attempts = 0;
	unsigned int dive_start_time = 0;
	unsigned int altim_silent_count = 0;

	double prev_z_goal_reached = 0.0;

	zvt_vector density_map;
	std::vector<double> piston_moves;

	SimSocket density_skt(&env_addr);

	while (true)
	{
		sim_time.sleep(ctd_wait_time[dive_state]);

		if (!enable_dive_ctrl.get())
		{
			sim_time.suspend_sleep();
			density_set = -1.0;
		}
		enable_dive_ctrl.wait_for(true);
		sim_time.enable_sleep();

		unsigned char echo = get_echo();
		ctd_data_t ctd_now = get_ctd();
		if (ctd_now.pressure == 0)
			continue;

		double density_now = get_float_density();
		if (density_now <= 0.0)
			continue;
		if (density_set <= 0.0)
			density_set = density_now;

		ctd.add(ctd_now);
		DepthState ds = ctd.get_depth_state();

		if (echo != ALTIM_SILENCE)
		{
			bottom_depth.set(ds.depth_now + 0.1 * echo);
			altim_silent_count = 0;
		}
		else if ((++altim_silent_count > 5) && (bottom_depth.get() < ds.depth_now + ALTIM_MAX_CONF_DIST))
			bottom_depth.set(ds.depth_now + ALTIM_MAX_CONF_DIST);

		double z_bot = bottom_depth.get() - bottom_min_distance.get();
		double z_max = bottom_depth.get() - bottom_near_distance.get();
		double z_goal = depth_goal.get();
		double z_tol = depth_tolerance.get();

		bool got_new_goal = (z_goal != prev_z_goal);
		if (got_new_goal)
		{
			if (!(
					(prev_z_goal <= DRIFTER_SURFACE_DEPTH)
						? ((ds.slope < 0.01) && (ds.depth_now <= DRIFTER_SURFACE_DEPTH))
						: ((ds.depth_now >= prev_z_goal - z_tol) && (ds.depth_now <= prev_z_goal + z_tol))))
			{
				unsigned int dpn = piston_moves.size();
				double dps = 0.0;
				if (dpn > 1)
				{
					double dpd = 0.0;
					for (unsigned int i = 0; i < dpn; ++i)
					{
						dpd += piston_moves[i];
						dps += fabs(piston_moves[i]);
					}
					dps -= fabs(dpd);
				}
				//dprint("!\t%.1f\t%u\t%.3f\n", prev_z_goal, dpn, dps);
			}

			dprint_dim("## dive to %.1fm from %.1fm\n", z_goal, ds.depth_now);
			prev_z_goal = z_goal;
			piston_moves.clear();
		}

		double prev_bottom_depth_adjustment = bottom_depth_adjustment;
		if (z_goal > z_max)
		{
			bottom_depth_adjustment = z_max - z_goal;
			z_goal = z_max;
		}
		else
			bottom_depth_adjustment = 0.0;
		bool notable_adjustment = fabs(bottom_depth_adjustment - prev_bottom_depth_adjustment) > 1.0;

		if (notable_adjustment)
			dprint_dim("bottom %.1fm >> goal %.1f%+.1fm = %.1fm\n",
					   bottom_depth.get(), z_goal - bottom_depth_adjustment, bottom_depth_adjustment, z_goal);

		// heuristics
		//bool prev_near_z_goal = near_z_goal;
		near_z_goal = (ds.depth_now >= z_goal - z_tol) && (ds.depth_now <= z_goal + z_tol);
		bool goal_at_surface = z_goal <= DRIFTER_SURFACE_DEPTH;
		bool at_surface = (ds.slope < 0.01) && (ds.depth_now <= DRIFTER_SURFACE_DEPTH);
		bool at_z_goal = goal_at_surface ? at_surface : near_z_goal;
		bool ds_good = (ds.time_valid > 4 * ctd_wait_time[dive_state]) && (ds.variance < 0.2);
		bool flat_now = ds_good && (fabs(ds.slope) < 0.001);
		bool soon_to_hit_bottom = ds.depth_now + ds.slope * 60 > z_bot;
		bool piston_moving = (fabs(density_now - density_set) > 1e-3);

		if (soon_to_hit_bottom)
		{
			if (density_set < density_now - 1e-3)
				continue;
		}
		else if (piston_moving)
			continue;

		//if (prev_near_z_goal != near_z_goal) dprint("goal %s at %+.1fm\n", near_z_goal ? "near" : "far", z_goal - ds.depth_now);

		// update density map
		if (((dive_state == adjusting_depth) || !at_z_goal) && flat_now)
		{
			//printf(" [D(%.2f) %.3f] ds %i", ds.depth_now, density_now - 1000, (int)density_map.size() + 1);
			density_map.push_back(zvt_struct(ds.depth_now, density_now, ctd_now.time));
		}

		// state transitions
		if ((dive_state == stable_depth) && (!at_z_goal || got_new_goal))
		{
			// start dive
			dive_state = adjusting_depth;

			dive_start_time = ctd_now.time;
			piston_moves.clear();
		}
		else if ((dive_state == adjusting_depth) && ((flat_now && at_z_goal) || (at_surface && goal_at_surface)))
		{
			// end dive
			dive_state = stable_depth;
			//dprint("== at goal (~%.1f m)\n", ds.depth_now);
			unsigned int dpn = piston_moves.size();
			double dps = 0.0;
			if (dpn > 1)
			{
				double dpd = 0.0;
				for (unsigned int i = 0; i < dpn; ++i)
				{
					dpd += piston_moves[i];
					dps += fabs(piston_moves[i]);
				}
				dps -= fabs(dpd);
			}
			//dprint("%c\t%.1f\t%u\t%.3f\n", (prev_z_goal_reached == z_goal ? '_' : '='), z_goal, dpn, dps);
			if (prev_z_goal_reached != z_goal)
			{
				dprint_dim("== at goal (~%.1f m): ", ds.depth_now);
				if (dpn <= 1)
					//printf(VT_SET(VT_DIM) "1 move\n" VT_RESET);
					printf("1 move\n");
				else
				{
					double dpd = 0.0, dps = 0.0;
					for (unsigned int i = 0; i < dpn; ++i)
					{
						dpd += piston_moves[i];
						dps += fabs(piston_moves[i]);
					}
					//printf(VT_SET(VT_BRIGHT) "%u moves, %.3f extra\n" VT_RESET, dpn, dps - fabs(dpd));
					printf("%u moves, %.3f extra\n", dpn, dps - fabs(dpd));
				}
			}
			//print_envstate(); putchar('\n');
			//compare_profile();

			adjust_temperature_estimate(dive_start_time);
			adjust_salinity_estimate(density_map);

			density_map.clear();
			surface_escape_attempts = 0;
			prev_z_goal_reached = z_goal;
		}

		//if (flat_now && !at_z_goal) dprint("== at %.1f m\n", ds.depth_now);

		bool should_move_piston =
			soon_to_hit_bottom || ((dive_state != stable_depth) && ((flat_now && !at_z_goal) || (ds_good && at_surface) || (dive_start_time >= ctd_now.time) || notable_adjustment));
		if (!should_move_piston)
			continue;

		//dprint("%4.1f", ds.depth_now);
		//if (!at_surface && !flat_now) printf(" %+.1f cm/s", ds.slope * 100);
		//printf(" :: %+.1fm", z_goal - ds.depth_now);

		double density_goal = estimate_density(z_goal, density_map, dive_start_time);
		//printf(" D %+.3f (%.3f)", density_goal - density_now, density_goal - 1000);

		// adjust for the surface
		if (goal_at_surface)
		{
			density_goal -= 0.2;
			if (density_now < density_goal)
				density_goal = density_now;
		}
		else if (at_surface && (++surface_escape_attempts > 2))
			density_goal += 0.1;
		else
			surface_escape_attempts = 0;

		// sanity check: make sure the move will be in the right direction
		if (!at_surface && !(notable_adjustment && (density_goal <= density_now)) && ((fabs(density_goal - density_now) < 1e-3) || ((z_goal > ds.depth_now) ^ (density_goal > density_now))))
		{
			//printf("<!D %+.3f>\n", density_goal - density_now);
			density_goal = density_now + (z_goal > ds.depth_now ? 1e-3 : -1e-3);
		}

		if (soon_to_hit_bottom)
		{
			double panic_goal = estimate_density(z_max, density_map, dive_start_time);
			if (panic_goal > density_now - 0.1)
				panic_goal = density_now - 0.1;
			//printf("<!B %+.1fm D %.3f>\n", bottom_depth.get() - ds.depth_now, ds.slope * 100, panic_goal - 1000);
			if (panic_goal < density_goal)
				density_goal = panic_goal;
		}

		ctd.reset();
		if (set_float_density(density_goal, &density_skt))
		{
			piston_moves.push_back(density_goal - density_set);
			density_set = density_goal;
		}
		else
			printf("<!set_density FAIL>\n");
		//printf(" >> D %.3f\n", density_goal - 1000);

		//for (zvt_vector::const_iterator it = density_map.begin(); it != density_map.end(); ++it) dprint("\t%.2f: %.2f\n", it->depth, it->value);

		//print_status();
	}

	return NULL;
}
