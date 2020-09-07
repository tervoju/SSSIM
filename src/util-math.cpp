/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cmath>



double tri_rand() {
	double u = drand48();
	return (u <= 0.5)
		? sqrt(2.0 * u) - 1.0
		: 1.0 - sqrt(2.0 * (1.0 - u));
}

double norm_rand() {
	// using Box-Muller transform
	static bool r2_ok = false;
	static double a = 0.0, d = 0.0;

	if (r2_ok) { r2_ok = false; return d * sin(a); }

	a = 2.0 * M_PI * drand48();
	d = sqrt(-2.0 * log(drand48()));
	r2_ok = true;
	return d * cos(a);
}

double exponential_rand() {
	//if (k <= 0.0) return -1.0;

	double u = drand48();
	if (u == 0.0) return 0.0; // would be infinity; actual range should include 0 but not infinity
	return -log(u);
}

double pareto_rand(double k) {
	if (k <= 0.0) return -1.0;

	double u = drand48();
	if (u == 0.0) return 1.0;
	if (k == 1.0) return 1.0 / u;
	return pow(u, -1.0 / k); 
}

bool below_threshold(double v, double lim, double lim_w) {
	if (v <= lim - lim_w) return true;
	else if (v >= lim + lim_w) return false;
	else {
		double x = (v - lim) / lim_w;
		double t = 0.5 + x * (1.0 - fabs(x) / 2.0); // sigmoid function approximation
		return drand48() >= t;
	}
}


//-----------------------------------------------------------------------------
double polyval(const double * p, int n, double x) {
	if (n < 1.0) return 0.0;
	double r = *p;
	while(--n) r = r * x + *(++p);
	return r;
}


//-----------------------------------------------------------------------------
double var_mult(double x_mean, double x_var, double y_mean, double y_var) {
	return x_var * (y_var + y_mean*y_mean) + y_var * x_mean*x_mean;
}

double var_div(double x_mean, double x_var, double y_mean, double y_var) {
	return (x_var * y_mean*y_mean - y_var * x_mean*x_mean) / (y_mean*y_mean * (y_var + y_mean*y_mean));
}
