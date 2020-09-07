/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util-leastsquares.h"

void LeastSquaresTracker::add(double t, double v) {
	if (n==0) t0 = t;
	++n;
	t -= t0;
	s_t += t;
	s_t2 += t*t;
	s_v += v;
	s_v2 += v*v;
	s_tv += t*v;
}

void LeastSquaresTracker::remove(double t, double v) {
	if (n==0) return;
	--n;
	t -= t0;
	s_t -= t;
	s_t2 -= t*t;
	s_v -= v;
	s_v2 -= v*v;
	s_tv -= t*v;
}

void LeastSquaresTracker::reset() {
	n = 0;
	t0 = 0.0;
	s_t = 0.0;
	s_t2 = 0.0;
	s_v = 0.0;
	s_v2 = 0.0;
	s_tv = 0.0;
}

bool LeastSquaresTracker::get_state(double & a, double & b, double & s2) const {
	if (n < 3) return false;
	double i_ss_tv = ss_tv();
	double i_ss_tt = ss_tt();
	if (i_ss_tt == 0.0) return false;
	b = i_ss_tv / i_ss_tt;
	a = (s_v - b * s_t) / n;
	s2 = (ss_vv() - b * i_ss_tv) / (n-2);
	return true;
}
