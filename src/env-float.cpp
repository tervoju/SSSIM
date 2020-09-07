/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <svl/SVL.h>

#include "util-math.h"
#include "util-convert.h"
#include "sssim.h"
#include "sssim-structs.h"
#include "sea-data.h"
#include "env-time.h"
#include "env-sea.h"
#include "env-float.h"

extern simtime_t env_time;
extern Sea *sea;

//-----------------------------------------------------------------------------
template <class T>
T *typed_malloc()
{
	void *buf = malloc(sizeof(T));
	memset(buf, 0, sizeof(T));
	return reinterpret_cast<T *>(buf);
}

//-----------------------------------------------------------------------------
SimFloat::SimFloat(int16_t _id, const FloatState &fs) : id(_id),
														frozen(false),
														pos(fs.pos),
														vel(fs.vel),
														volume(fs.volume),
														volume_goal(fs.volume),
														bottom_depth(0.0),
														salinity(0.0),
														temperature(0.0),
														rho(1.0),
														drift(vl_zero)
{
	if (!pos[2])
		pos[2] = DRIFTER_HALF_HEIGHT;

	if (pos[0] && pos[1])
	{
		if ((pos[0] < 100.0) && (pos[1] < 100.0))
		{
			pos[0] = degrees_east_to_meters(pos[0]);
			pos[1] = degrees_north_to_meters(pos[1]);
		}
		UpdateEnvironment();
	}
	else
		RandomizeWithMapCenter(0.2, 0.2, 0.003, 0.003); // ~1km square

	if (!volume)
		volume_goal = volume = DRIFTER_MASS / rho;
	else
	{
		// check that "volume" isn't density
		const double x = DRIFTER_MASS / volume;
		if (x < 100.0)
			volume_goal = volume = x;
	}
}

//-----------------------------------------------------------------------------
void SimFloat::RandomizeWithMapCenter(double x0, double y0, double xr, double yr)
{
	const double
		sea_w = (SEA_NX - 1) * sea->step.x,
		sea_h = (SEA_NY - 1) * sea->step.y;
	do
	{
		pos[0] = sea->min.x + sea_w * (x0 + xr * (2 * drand48() - 1));
		pos[1] = sea->min.y + sea_h * (y0 + yr * (2 * drand48() - 1));
		//pos[2] = DRIFTER_HALF_HEIGHT;
	} while (!UpdateEnvironment() || (bottom_depth < 20.0));
}

//-----------------------------------------------------------------------------
void *SimFloat::get_info(size_t *len) const
{
	FloatState *f = typed_malloc<FloatState>();

	f->pos = pos;
	f->vel = vel;
	f->volume = volume;
	f->bottom_depth = bottom_depth;
	f->id = id;

	if (len != NULL)
		*len = sizeof(*f);
	return f;
}

void *SimFloat::get_env(size_t *len) const
{
	T_EnvData *e = typed_malloc<T_EnvData>();

	e->depth = bottom_depth;
	e->salinity = salinity;
	e->temperature = temperature;
	e->drift_x = drift[0];
	e->drift_y = drift[1];

	if (len != NULL)
		*len = sizeof(*e);
	return e;
}

void *SimFloat::get_env(const double depth, size_t *len) const
{
	T_EnvData *e = typed_malloc<T_EnvData>();

	double z_pos[3], z_drift[3];
	z_pos[0] = pos[0];
	z_pos[1] = pos[1];
	z_pos[2] = depth;
	e->depth = sea->bottom(z_pos);
	sea->variables(z_pos, env_time, &e->salinity, &e->temperature, z_drift);
	e->drift_x = z_drift[0];
	e->drift_y = z_drift[1];

	if (len != NULL)
		*len = sizeof(*e);
	return e;
}

//-----------------------------------------------------------------------------
void *SimFloat::get_ctd(size_t *len) const
{
	ctd_data_t *c = typed_malloc<ctd_data_t>();

	unsigned int gauge_pressure = pressure_from_depth(pos[2]);
	c->conductivity = conductivity_from_STD(salinity, temperature, gauge_pressure);
	c->temperature = temperature;
	c->pressure = gauge_pressure + STANDARD_ATMOSPHERIC_PRESSURE;
	c->time = env_time;

	if (len != NULL)
		*len = sizeof(*c);
	return c;
}

void *SimFloat::get_echo(size_t *len) const
{
	unsigned char *d = typed_malloc<unsigned char>();

	double dist = bottom_depth - pos[2] - DRIFTER_HALF_HEIGHT;
	*d = (dist < 0.0) ? 0
					  : (dist > ALTIM_MAX_DIST) ? ALTIM_SILENCE
												: dist * 10;

	if (len != NULL)
		*len = sizeof(*d);
	return d;
}

void *SimFloat::get_density(size_t *len) const
{
	double *d = typed_malloc<double>();

	*d = (volume > 0) ? DRIFTER_MASS / volume : rho;

	if (len != NULL)
		*len = sizeof(*d);
	return d;
}

void *SimFloat::get_gps(size_t *len) const
{
	gps_data_t *g = typed_malloc<gps_data_t>();

	g->t = env_time;
	g->ok = (pos[2] < DRIFTER_HALF_HEIGHT + 0.2);
	if (g->ok)
	{
		g->x = pos[0];
		g->y = pos[1];
	}

	if (len != NULL)
		*len = sizeof(*g);
	return g;
}

//-----------------------------------------------------------------------------
bool SimFloat::in_sound_channel(double *ss) const
{
	double ss_self = soundspeed();
	if (ss != NULL)
		*ss = ss_self;

	double z_pos[3] = {pos[0], pos[1], 0.0};

	double bottom_depth = sea->bottom(z_pos);
	if (bottom_depth <= 0.0)
		return false;

	double ss_min = sea->min_soundspeed(z_pos, env_time, bottom_depth);
	if (ss_min <= 0.0)
		return false;
	double ss_sc_max = ss_min + 2.0;

	z_pos[2] = bottom_depth - DRIFTER_HALF_HEIGHT;
	double ss_bottom = sea->soundspeed(z_pos, env_time);
	if ((ss_bottom > 0.0) && (ss_sc_max > ss_bottom))
		ss_sc_max = ss_bottom;

	return below_threshold(ss_self, ss_sc_max, 0.5);
}

//-----------------------------------------------------------------------------
bool SimFloat::set_density(double density)
{
	if (density > 0.0)
	{
		volume_goal = DRIFTER_MASS / density;
		return true;
	}
	else
		return false;
}

//-----------------------------------------------------------------------------
void SimFloat::Update()
{
	if (frozen)
		return;

	const double dt = SIMULATOR_STEPSIZE;

	bool ok = UpdateEnvironment();

	if (!ok)
	{
		print_timestr();
		printf("float %i: stopped, out of area\n", id);
	}
	else if (bottom_depth < 2 * DRIFTER_HALF_HEIGHT)
	{
		print_timestr();
		printf("float %i: stopped, beached (depth %.2fm)\n", id, bottom_depth);
		ok = false;
	}
	else
	{
		UpdateVolume(dt);
		StepRungeKutta4(dt);
	}

	if (!ok)
	{
		frozen = true;
		print_timestr();
		printf("\tat %.2f E, %.2f N, %.1f m\n", meters_east_to_degrees(pos[0]), meters_north_to_degrees(pos[1]), pos[2]);
	}
}

void SimFloat::UpdateVolume(double dt)
{
	if (volume_goal == volume)
		return;

	double
		voldiff = volume_goal - volume,
		abs_dv = dt * DRIFTER_VOLUME_SPEED;
	if (fabs(voldiff) < std::max(abs_dv, 0.1 * DRIFTER_VOLUME_SPEED))
		volume = volume_goal;
	else
		volume += SIGN(voldiff) * abs_dv;
}

bool SimFloat::UpdateEnvironment()
{
	bottom_depth = sea->bottom(pos.Ref());
	const bool on_map = sea->variables(pos.Ref(), env_time, &salinity, &temperature, drift.Ref());

	if (on_map)
		rho = density_from_STD(salinity, temperature, pressure_from_depth(pos[2]));

	/*printf("%s: pos:%.4lf,%.4lf, depth:%.1lf, bottom:%.1lf, salt:%.3lf, temp:%.3lf, on_map:%d\n", __FUNCTION__, 
		meters_east_to_degrees(pos[0]), meters_north_to_degrees(pos[1]), 
		pos[2], bottom_depth, salinity, temperature, on_map ? 1 : 0);*/
	return on_map;
}

Vec3 SimFloat::Accelerate3D(Vec3 v, double depth)
{
	double hf = -rho * DRIFTER_HORIZONTAL_DRAG_MULTIPLIER * sqrt(v[0] * v[0] + v[1] * v[1]);
	double adj_volume = volume;
	if (depth < DRIFTER_NEAR_SURFACE_DEPTH)
	{
		// movement near surface has much higher frequency than other movement,
		// so need to limit forces by capping volume difference
		// depth checked to allow rapid ascent for CTD profiling
		if (adj_volume > DRIFTER_MAX_VOLUME_NEAR_SURFACE)
			adj_volume = DRIFTER_MAX_VOLUME_NEAR_SURFACE;
	}
	return Vec3(
		hf * v[0],
		hf * v[1],
		GRAVITY - rho * (GRAVITY / DRIFTER_MASS * adj_volume + DRIFTER_VERTICAL_DRAG_MULTIPLIER * v[2] * fabs(v[2])));
}

#define RK4_MAX_STEP_LENGTH_FRACTION 0.8

void SimFloat::StepRungeKutta4(double td)
{
	double half_td = td / 2;
	double d0 = pos[2], dd0 = vel[2];
	Vec3 vd = vel - drift;
	double v_max_sqrlen = RK4_MAX_STEP_LENGTH_FRACTION * sqrlen(vd);

	Vec3 k[4];
	k[0] = half_td * Accelerate3D(vd, d0);
	double k0_sqrlen = sqrlen(k[0]);
	if (k0_sqrlen > v_max_sqrlen)
	{
		int n = ceil(sqrt(k0_sqrlen / v_max_sqrlen));
		double t_n = td / n;
		for (int i = 0; i < n; ++i)
			StepRungeKutta4(t_n);
		return;
	}
	k[1] = half_td * Accelerate3D(vd + k[0], d0 + half_td * dd0);
	k[2] = half_td * Accelerate3D(vd + k[1], d0 + half_td * (dd0 + k[0][2]));
	k[3] = half_td * Accelerate3D(vd + 2 * k[2], d0 + td * (dd0 + k[1][2]));

	vel += (k[0] + 2 * k[1] + 2 * k[2] + k[3]) / 3;
	pos += td * (vel + (k[0] + k[1] + k[2]) / 3);

	if (pos[2] > bottom_depth - 0.01 - DRIFTER_HALF_HEIGHT)
	{
		print_timestr();
		printf("float %i: hit bottom at %+.2fcm/s\n", id, vel[2] * 100);
		print_timestr();
		printf("at %.2f E, %.2f N, %.1f m\n", meters_east_to_degrees(pos[0]), meters_north_to_degrees(pos[1]), pos[2]);
		pos[2] = bottom_depth - 0.02 - DRIFTER_HALF_HEIGHT;
		if (vel[2] > 0.0)
			vel[2] = 0.0;
	}
	if (pos[2] < DRIFTER_HALF_HEIGHT)
	{
		pos[2] = DRIFTER_HALF_HEIGHT;
		if (vel[2] < 0.0)
			vel[2] = 0.0;
	}
}

void SimFloat::StepEuler(double td)
{
	vel += td * Accelerate3D(vel - drift, pos[2]);
	vel[0] = drift[0];
	vel[1] = drift[1];
	pos += td * vel;
}

/*
std::ostream & operator << (std::ostream &s, const SimFloat &d) {
	FloatState fs;
	d.GetStatus(fs);
	return s << '#' << d.id << ": " << fs;
}
*/
