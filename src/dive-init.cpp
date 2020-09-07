/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <vector>
#include <stack>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "util-leastsquares.h"
#include "util-convert.h"
#include "util-tsdouble.h"
#include "util-trigger.h"
#include "sssim.h"
#include "udp.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"
#include "client-print.h"
#include "ctd-tracker.h"
#include "ctd-estimate.h"
#include "float-util.h"
#include "satmsg-fmt.h"

extern SimTime sim_time;

extern double salt_est[];
extern ts_double bottom_depth;
extern ts_double bottom_min_distance;

extern trigger_t enable_dive_ctrl;

//-----------------------------------------------------------------------------
class dive_init_t
{
  public:
	dive_init_t(const init_params_t *_conf);

	void start();
	bool loop();
	void end();

	zvt_vector density_map;

  private:
	dive_init_t(const dive_init_t &di2);
	dive_init_t &operator=(const dive_init_t &di2);

	const init_params_t *conf;

	simtime_t dive_start_time;
	simtime_t time_elapsed;

	int found_bottom;

	double d_surface;
	double d_set;
	std::stack<double> rise_densities;

	void _set_salinity_estimate();

	void set_density(double d);
	void step_down();
	void start_dive_up();
	void step_up(double d_max);
};

dive_init_t::dive_init_t(const init_params_t *_conf) : density_map(),
													   conf(_conf),
													   dive_start_time(),
													   time_elapsed(),
													   found_bottom(),
													   d_surface(),
													   d_set(),
													   rise_densities()
{
}

//-----------------------------------------------------------------------------
void dive_init_t::set_density(double d)
{
	if (d == d_set)
		return;
	//dprint("D %.3f\n", d);
	ctd.reset();
	if (set_float_density(d))
		d_set = d;
}

//-----------------------------------------------------------------------------
void dive_init_t::step_down()
{
	set_density(d_set + conf->d_step);
	//dprint("> diving down\n");
}

//-----------------------------------------------------------------------------
void dive_init_t::start_dive_up()
{
	density_filter(density_map);
	int time_remaining = static_cast<int>(conf->t_length) - time_elapsed;
	if ((time_remaining < static_cast<int>(conf->t_buffer)) || density_map.empty() || !rise_densities.empty())
		return;

	unsigned int gaps = density_map.size() - 1;

	double step_time;
	switch (gaps)
	{
	case 0:
		step_time = 20.0 * 60;
		break;
	case 1:
		step_time = density_map[1].timestamp - density_map[0].timestamp;
		break;
	default:
	{
		int zi0 = (density_map[0].depth <= DRIFTER_SURFACE_DEPTH) ? 1 : 0;
		step_time = static_cast<double>(density_map.back().timestamp - density_map[zi0].timestamp) / (gaps - zi0);
	}
	}
	uint32_t steps = time_remaining / step_time;

	//dprint("step time %.2f min; total steps %i + %i = %i\n", step_time/60.0, gaps+1, steps, gaps+steps+1);

	unsigned int min_steps_in_each_gap = steps / gaps;
	unsigned int extra_steps = steps - min_steps_in_each_gap * gaps;

	bool first_gap = true;
	zvt_iter it = density_map.begin();
	while (gaps--)
	{
		double d0 = it->value;
		++it;
		double d1 = it->value;
		unsigned int div = min_steps_in_each_gap + 1;
		if (extra_steps)
		{
			++div;
			--extra_steps;
			if (extra_steps && first_gap)
			{
				++div;
				--extra_steps;
			}
		}
		first_gap = false;
		double dd = (d1 - d0) / div;
		for (unsigned int i = 1; i < div; ++i)
		{
			double di = d0 + i * dd;
			rise_densities.push(di);
			//dprint("\t%.3f\n", di);
		}
		if (min_steps_in_each_gap + extra_steps == 0)
			break;
	}
}

void dive_init_t::step_up(double d_max)
{
	if (abs(found_bottom) < 2)
	{
		found_bottom *= 2;
		start_dive_up();
	}

	double d_new = d_surface;
	while (!rise_densities.empty())
	{
		d_new = rise_densities.top();
		rise_densities.pop();
		if (d_new <= d_max)
			break;

		d_new = d_surface;
	}
	if ((d_new == d_surface) && (d_new > d_max))
		d_new = d_max;

	set_density(d_new);

	//dprint("> diving up\n");
}

//-----------------------------------------------------------------------------
bool dive_init_t::loop()
{
	unsigned char echo = get_echo();
	ctd_data_t ctd_now = get_ctd();
	//dprint("CTD %.1f mS/cm, %.1f°C, %u mbar -> %.1f m\n",
	//ctd_now.conductivity, ctd_now.temperature, ctd_now.pressure,
	//depth_from_pressure(ctd_now.pressure - STANDARD_ATMOSPHERIC_PRESSURE));
	if (ctd_now.pressure == 0)
		return true;

	double density_now = get_float_density();
	if (density_now <= 0.0)
		return true;

	ctd.add(ctd_now);
	DepthState ds = ctd.get_depth_state();
	//dprint("DS %.1fm %+.2fcm/s, v %.2f @ %.0f s\n", ds.depth_now, ds.slope * 100, ds.variance, ds.time_valid);

	time_elapsed = sim_time.now() - dive_start_time;
	//dprint(">< %u %.3f %.1f\n", time_elapsed, density_now, ds.depth_now);

	// heuristics
	bool piston_moving = (fabs(density_now - d_set) > 1e-3);
	bool at_surface = (ds.slope < 0.01) && (ds.depth_now <= DRIFTER_SURFACE_DEPTH);
	bool ds_good = (ds.time_valid > 120) && (ds.variance < 0.2);
	bool flat_now = ds_good && (fabs(ds.slope) < 0.001);

	if (echo != ALTIM_SILENCE)
	{
		const double z_bottom = ds.depth_now + 0.1 * echo;
		bottom_depth.set(z_bottom);
		if (found_bottom <= 0)
		{
			dprint("env init: bottom at %.1fm (%+.1fm)\n", z_bottom, z_bottom - ds.depth_now);
			found_bottom = 1;
		}
	}
	else if (!found_bottom && (ds.depth_now >= conf->z_max))
	{
		dprint("env init: reached max depth (%.1fm)\n", ds.depth_now);
		if (!found_bottom)
			found_bottom = -1;
	}

	// bottom safety
	if (found_bottom > 0)
	{
		const double z_bottom = bottom_depth.get();
		bool we_should_panic = (d_set > density_now - 1e-3) && (ds.depth_now + ds.slope * 60 > z_bottom - bottom_min_distance.get());
		if (we_should_panic)
		{
			double d_new = d_set - fmod(d_set - conf->d_start - 1e-5, conf->d_step) / 2;
			if (d_new > density_now - 0.1)
				d_new = density_now - 0.1;
			set_density(d_new);
			if (ds.depth_now > z_bottom - 1.0)
				dprint(
					"env init: bottom at %.1fm (%+.1fm); v %+.1fcm/s\n",
					z_bottom, z_bottom - ds.depth_now,
					ds.slope * 100);
			//dprint("> diving up\n");
			return true;
		}
	}

	if (piston_moving)
		return true;

	if (!density_map.empty())
	{
		zvt_struct &d0 = density_map.front();
		if ((d_surface != d0.value) && (d0.depth <= DRIFTER_SURFACE_DEPTH))
			d_surface = d0.value;
	}

	if (!at_surface && (time_elapsed >= conf->t_length - conf->t_buffer) && ((d_set > d_surface) || flat_now))
	{
		while (!rise_densities.empty())
			rise_densities.pop();
		if (!found_bottom)
			found_bottom = -1;
		dprint("env init: out of time, %.1fmin left\n", (static_cast<double>(conf->t_length) - time_elapsed) / 60.0);
		if (!flat_now)
		{
			step_up(density_now - 0.1);
			return true;
		}
	}

	if (flat_now || (ds_good && at_surface))
	{
		density_map.push_back(zvt_struct(ds.depth_now, density_now, ctd_now.time));

		if (at_surface)
		{
			if (found_bottom)
				return false; // we're done.
			else
				dprint("env init: at surface (%.2f kg/m³)\n", d_set);
		}
		else
			dprint("env init: at %.1fm (%.2f kg/m³)\n", ds.depth_now, d_set);

		if (found_bottom)
			step_up(density_now - 0.1);
		else
			step_down();
	}

	return true;
}

//-----------------------------------------------------------------------------
void dive_init_t::start()
{
	d_set = get_float_density();
	dive_start_time = sim_time.now();
	density_map.push_back(zvt_struct(DRIFTER_HALF_HEIGHT, d_set, dive_start_time));

	set_density(conf->d_start);
	dprint("env init: %.1f : %.1f : %.1f kg/m³ (%um) in %.1f (-%.1f) min\n",
		   conf->d_start, conf->d_step, conf->d_end,
		   conf->z_max,
		   conf->t_length / 60.0, conf->t_buffer / 60.0);
}

//-----------------------------------------------------------------------------
void dive_init_t::_set_salinity_estimate()
{
	density_filter(density_map);
	zvt_vector salt_map = salt_vector(density_map);

	if (salt_map.empty())
		return;
	else if (salt_map.size() < 2)
	{
		adjust_salinity_estimate(density_map);
		return;
	}

	zvt_const_iter it = salt_map.begin();
	int zi = 0;
	double
		z0 = it->depth,
		s0 = it->value,
		ds_dz = 0.0;
	for (++it; ((it != salt_map.end()) && (zi < zvar_count)); ++it)
	{
		double
			z1 = it->depth,
			s1 = it->value;
		ds_dz = (s1 - s0) / (z1 - z0);
		for (; ((zi < z1) && (zi < zvar_count)); ++zi)
			salt_est[zi] = s1 - (z1 - zi) * ds_dz;
		z0 = z1;
		s0 = s1;
	}
	for (; (zi < zvar_count); ++zi)
		salt_est[zi] = s0 + (zi - z0) * ds_dz;
}

//-----------------------------------------------------------------------------
void dive_init_t::end()
{
	adjust_temperature_estimate(dive_start_time, false);
	_set_salinity_estimate();

	dprint("env init: done in %.1fmin with %i stable depths\n", time_elapsed / 60.0, density_map.size());
	//FOREACH_CONST(it, density_map) dprint("\t%.2f: %.2f\n", it->depth, it->value);
}

//-----------------------------------------------------------------------------
void dive_init_wrapper(const init_params_t *conf, std::vector<double> *depths)
{
	dive_init_t dive_init(conf);

	enable_dive_ctrl.set(false);

	dive_init.start();
	while (dive_init.loop())
		sim_time.sleep(30);
	dive_init.end();

	enable_dive_ctrl.set(true);

	if (depths)
	{
		depths->clear();
		depths->reserve(dive_init.density_map.size());
		for (zvt_vector::const_iterator it = dive_init.density_map.begin(); it != dive_init.density_map.end(); ++it)
			depths->push_back(it->depth);
	}
}
