/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <deque>
#include <pthread.h>
#include <stdint.h>
//#include <stdio.h> // DEBUG for printf

#include "util-leastsquares.h"
#include "util-convert.h"

#include "ctd-tracker.h"

#define  METERS_PER_MILLIBAR  0.010085

CTDTracker ctd;


/**
 * Chi-square distribution with n degrees of freedom
 * used for checking probability of v (normalized)
 *
 * with precalculated numbers found using Distribution Calculator
 * from http://www.vias.org/simulations/simusoft_distcalc.html
 */
bool chi_square_crit_range_check(unsigned int n, double v) {
	// bool doubleEnded = true;
	// double significanceLevel = 1.0e-5;

	switch(n) {
		case 1:
			return (v < 23.9355);  // single-ended, level 1e-6 (1e-5: 19.5132)
		case 5:
			return ((v > 0.0246) && (v < 32.3761));
		case 20:
			return ((v > 3.0694) && (v < 60.9879));
		case 50:
			return ((v > 17.4178) && (v < 107.0165));
		default:
			return false;  // fail on uncalculated deg. of freedom
	}
}


//-----------------------------------------------------------------------------
CTDTracker::CTDTracker():
	max_size(1000),
	max_variance(10000),
	pred_max_length(80),
	depth_state(),
	mutex(),
	surfacing_time(0),
	lst(),
	atm_p(STANDARD_ATMOSPHERIC_PRESSURE),
	data()
{
	pthread_mutex_init(&mutex, NULL);
	pred_length[0] =  1; pred_min_length[0] =  7; // no proper reasons for exactly these pred_min_lengths,
	pred_length[1] =  5; pred_min_length[1] = 20; // they depend on signal noise autocorrelation
	pred_length[2] = 20; pred_min_length[2] = 40;
	depth_state.depth_now = -1.0;
}

CTDTracker::~CTDTracker() { pthread_mutex_destroy(&mutex); }


//-----------------------------------------------------------------------------
double CTDTracker::max_surf_p() {
	static double _atm_p = 0.0;
	static double _max_surf_p = 0.0;
	if (atm_p != _atm_p) {
		_atm_p = atm_p;
		_max_surf_p = atm_p + pressure_from_depth(DRIFTER_SURFACE_DEPTH);
	}
	return _max_surf_p;
}


//-----------------------------------------------------------------------------
bool CTDTracker::surface_ds_ok() {
	if (!surfacing_time) {
		surfacing_time = data[0].time;
		lst.reset();
	}

	depth_state.depth_now = depth(data[0].pressure);
	if (depth_state.depth_now > DRIFTER_SURFACE_DEPTH - 0.1) depth_state.depth_now = DRIFTER_SURFACE_DEPTH - 0.1;
	depth_state.slope = 0.0;
	depth_state.variance = 0.0;
	depth_state.time_valid = data[0].time - surfacing_time;
	return true;
}


//-----------------------------------------------------------------------------
bool CTDTracker::validate_range(uint32_t test_length, uint32_t test_end) {
	LeastSquaresTracker lst_base = lst;
	for (unsigned int j = test_end - test_length; j < test_end; ++j)
		lst_base.remove(data[j].time, data[j].pressure);

	double a, b, e_s2;
	if (!lst_base.get_state(a, b, e_s2)) return true;

	double i_s2 = 0.0;
	for (unsigned int j = test_end - test_length; j < test_end; ++j) {
		double e = data[j].pressure - (a + b * (data[j].time - lst.t0));
		i_s2 += e * e;
	}

	return (e_s2 == 0.0) ? (i_s2 <= e_s2)
		: chi_square_crit_range_check(test_length, i_s2/e_s2);
}

bool CTDTracker::validate_state(int * first_valid) {
	for (unsigned int i = 0; i < CTD_PRED_COUNT; ++i) {
		unsigned int & pl = pred_length[i];

		if (lst.n < pl + pred_min_length[i]) break;

		// latest
		if (!validate_range(pl, pl)) {
			//printf("[CTD invalid latest %u]\n", pl); // DEBUG
			*first_valid = (i < 1) ? 0 : pred_length[i-1] - 1;
			return false;
		}

		// earliest
		if (!validate_range(pl, lst.n)) {
			//printf("[CTD invalid earliest %u]\n", pl); // DEBUG
			*first_valid = pred_min_length[0] - 1;
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
bool CTDTracker::underwater_ds_ok() {
	if (surfacing_time) {
		if (data[0].pressure < atm_p + pressure_from_depth(DRIFTER_SURFACE_TOLERANCE_MAX_DEPTH)) {
			unsigned int n = DRIFTER_SURFACE_TOLERANCE_MIN_READINGS;
			if (data.size() < n) n = data.size();
			for (unsigned int i = 1; i < n; ++i)
				if (data[i].pressure < max_surf_p()) return surface_ds_ok();
		}
		surfacing_time = 0;
	}
	lst.add(data[0].time, data[0].pressure);

	// check fitness of latest & earliest measurement(s)
	int first_valid = 0;
	if (!validate_state(&first_valid)) {
		lst.reset();
		for (int j = first_valid; j >= 0; --j) lst.add(data[j].time, data[j].pressure);
	}

	// check variance
	double a, b, s2;
	if (lst.get_state(a, b, s2) && (s2 > max_variance)) {
		lst.reset();
		lst.add(data[0].time, data[0].pressure);
	}

	if (lst.n >= 3) {
		uint32_t t = data[0].time - lst.t0;
		double e_p = a + b * t;

		depth_state.depth_now = (e_p <= atm_p)
			? 0.0
			: depth(e_p);
		//double meters_per_millibar = depth(e_p+1) - depth_state.depth_now;
		depth_state.slope = b * METERS_PER_MILLIBAR;
		depth_state.variance = s2 * METERS_PER_MILLIBAR * METERS_PER_MILLIBAR;
		depth_state.time_valid = t;
		return true;
	} else {
		depth_state.depth_now = depth(data[0].pressure);
		depth_state.slope = 0.0;
		depth_state.variance = 0.0;
		depth_state.time_valid = 0.0;
		return false;
	}
}


//-----------------------------------------------------------------------------
bool CTDTracker::add(const ctd_data_t & ctd_new) {
	lock_data();
		while (lst.n >= pred_max_length) lst.remove(data[lst.n-1].time, data[lst.n-1].pressure);

		while (data.size() >= max_size) data.pop_back();
		data.push_front(ctd_new);

		bool ds_ok = (data[0].pressure < max_surf_p())
			? surface_ds_ok()
			: underwater_ds_ok();
	unlock_data();
	//printf("[CTD add %u, %u]\n", ctd_new.pressure, ctd_new.time); // DEBUG
	return ds_ok;
}


//-----------------------------------------------------------------------------
void CTDTracker::reset(bool clear_data) {
	lock_data();
		surfacing_time = 0;
		lst.reset();
		if (clear_data) {
			data.clear();
			depth_state.depth_now = -1.0;
			depth_state.time_valid = 0.0;
		}
	unlock_data();
	//printf("[CTD reset]\n"); // DEBUG
}


//-----------------------------------------------------------------------------
ctd_data_t CTDTracker::get_latest_ctd() {
	if (data.empty()) {
		ctd_data_t e = {0.0, 0.0, 0, 0};
		return e;
	}
	lock_data();
		ctd_data_t ctd0 = data[0];
	unlock_data();
	return ctd0;
}

DepthState CTDTracker::get_depth_state() {
	lock_data();
		DepthState ds = depth_state;
	unlock_data();
	return ds;
}

double CTDTracker::depth_now() {
	lock_data();
		double z = depth_state.depth_now;
	unlock_data();
	return z;
}

