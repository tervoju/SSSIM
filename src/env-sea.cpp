/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <stdint.h>
#include <netcdfcpp.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "sea-data.h"
#include "env-sea.h"
#include "util-math.h"
#include "util-convert.h"

//-----------------------------------------------------------------------------
void verify_NcFile(NcFile *file, unsigned int level, const char *s)
{
	try
	{
		if (!file->is_valid())
			throw 1;

		if (level < 1)
			return;
		if (file->get_dim(SEA_XVAR)->size() != SEA_NX_MAX)
			throw 2;
		if (file->get_dim(SEA_YVAR)->size() != SEA_NY_MAX)
			throw 3;

		if (level < 2)
			return;
		if (file->get_dim("T")->size() != SEA_NT)
			throw 4;

		if (level < 3)
			return;
		if (file->get_dim("Z")->size() != SEA_NZ)
			throw 5;
	}
	catch (int i)
	{
		printf("%s: bad data: %s [%i]\n", __FUNCTION__, s, i);
		exit(i);
	}
}

//-----------------------------------------------------------------------------
bool Sea::ReadVar(const char *var_name, const char *fn, sea_data_t &data)
{
	NcFile file(fn, NcFile::ReadOnly);
	verify_NcFile(&file, 3, var_name);

	NcVar *var;
	var = file.get_var(var_name);

	return var->set_cur(0, 0, 0, 0) && var->get(reinterpret_cast<double *>(&data), SEA_NT, SEA_NZ, SEA_NY, SEA_NX);
}

//-----------------------------------------------------------------------------
void Sea::ReadDimensions(const char *fn)
{
	NcFile file(fn, NcFile::ReadOnly);
	verify_NcFile(&file, 1, fn);

	double d[2];

	file.get_var(SEA_TVAR)->get(d, 2);
	min.t = 0.0; //d[0];
	step.t = d[1] - d[0];
	max_loc.t = SEA_NT - 1.000001;

	file.get_var(SEA_ZVAR)->get(depth_data, SEA_NZ);
	for (unsigned int z = 0; z < SEA_NZ; ++z)
		depth_data[z] *= -1;
	min.z = 0.0;  // unused
	step.z = 0.0; // unused
	max_loc.z = SEA_NZ - 2;

	file.get_var(SEA_YVAR)->get(d, 2);
	min.y = d[0];
	step.y = d[1] - d[0];
	min.y -= step.y / 2; // HACK to better match bathymetry
	max_loc.y = SEA_NY - 1.000001;

	file.get_var(SEA_XVAR)->get(d, 2);
	min.x = d[0];
	step.x = d[1] - d[0];
	max_loc.x = SEA_NX - 1.000001;
}

void Sea::AdjustRanges()
{
	min.x = degrees_east_to_meters(min.x);
	min.y = degrees_north_to_meters(min.y);
	step.x *= METERS_PER_DEGREE_EAST;
	step.y *= METERS_PER_DEGREE_NORTH;

	bottom_min.x = degrees_east_to_meters(bottom_min.x);
	bottom_min.y = degrees_north_to_meters(bottom_min.y);
	bottom_step.x *= METERS_PER_DEGREE_EAST;
	bottom_step.y *= METERS_PER_DEGREE_NORTH;
}

//-----------------------------------------------------------------------------
bool Sea::ReadDepth(const char *fn)
{ // sets bottom_data, bottom_min & bottom_step; requires unadjusted min & step
	NcFile file(fn, NcFile::ReadOnly);
	verify_NcFile(&file, 0, "Depth");

	NcVar *var;
	double d[2];

	var = file.get_var("IOWLON");
	var->set_cur(0, -1);
	var->get(d, 2);
	bottom_step.x = d[1] - d[0];
	const int bottom_x0 = (min.x - d[0]) / bottom_step.x - 0.1;
	const int bottom_nx = ((min.x + (SEA_NX - 1) * step.x - d[0]) / bottom_step.x + 2.1) - bottom_x0;
	var->set_cur(bottom_x0, -1);
	var->get(&bottom_min.x, 1);

	var = file.get_var("IOWLAT2");
	var->set_cur(0, -1);
	var->get(d, 2);
	bottom_step.y = d[1] - d[0];
	const int bottom_y0 = (min.y - d[0]) / bottom_step.y - 0.1;
	const int bottom_ny = ((min.y + (SEA_NY - 1) * step.y - d[0]) / bottom_step.y + 2.1) - bottom_y0;
	var->set_cur(bottom_y0, -1);
	var->get(&bottom_min.y, 1);

	if ((bottom_nx != SEA_DEPTH_NX) || (bottom_ny != SEA_DEPTH_NY))
	{
		printf("Sea bottom area changed. Please recompile with the following in src/env-sea.h:\n\n");
		printf("\t#define  SEA_DEPTH_NX  %i\n", bottom_nx);
		printf("\t#define  SEA_DEPTH_NY  %i\n\n", bottom_ny);
		exit(1);
	}

	var = file.get_var("BALDEP");

	return var->set_cur(bottom_y0, bottom_x0) && var->get(reinterpret_cast<double *>(bottom_data), bottom_ny, bottom_nx);
}

//-----------------------------------------------------------------------------
Sea::Sea() : min(), step(), max_loc(),
			 bottom_min(), bottom_step()
{
	ReadVar("S", SEA_DATA_DIR SEA_DATA_FILE_SALT, salt_data);
	ReadVar("TEMP", SEA_DATA_DIR SEA_DATA_FILE_TEMP, temp_data);
	ReadVar("U", SEA_DATA_DIR SEA_DATA_FILE_U_VEL, flow_data[X]);
	ReadVar("V", SEA_DATA_DIR SEA_DATA_FILE_V_VEL, flow_data[Y]);
	ReadVar("W", SEA_DATA_DIR SEA_DATA_FILE_W_VEL, flow_data[Z]);

	ReadDimensions(SEA_DATA_DIR SEA_DATA_FILE_TEMP);

	ReadDepth(SEA_DATA_FILEPATH_DEPTH);

	AdjustRanges();
}

//-----------------------------------------------------------------------------
double Sea::bottom(const double *pos) const
{
	double loc_x = (pos[X] - bottom_min.x) / bottom_step.x;
	loc_x = CLAMP(loc_x, 0.0, SEA_DEPTH_NX - 1.000001);
	double cell_fx, frac_x = modf(loc_x, &cell_fx);
	const unsigned int cell_x = cell_fx;

	double loc_y = (pos[Y] - bottom_min.y) / bottom_step.y;
	loc_y = CLAMP(loc_y, 0.0, SEA_DEPTH_NY - 1.000001);
	double cell_fy, frac_y = modf(loc_y, &cell_fy);
	const unsigned int cell_y = cell_fy;

	// printf("BOTTOM\n");
	// printf("\tX %f > %u/%f (min %f, step %f, cmax %u)\n", pos[X], cell_x, frac_x, bottom_min.x, bottom_step.x, SEA_DEPTH_NX - 2);
	// printf("\tY %f > %u/%f (min %f, step %f, cmax %u)\n", pos[Y], cell_y, frac_y, bottom_min.y, bottom_step.y, SEA_DEPTH_NY - 2);

	return -1.0 * interpolate(frac_y,
							  interpolate(frac_x, bottom_data[cell_y][cell_x], bottom_data[cell_y][cell_x + 1]),
							  interpolate(frac_x, bottom_data[cell_y + 1][cell_x], bottom_data[cell_y + 1][cell_x + 1]));
}

//-----------------------------------------------------------------------------
bool Sea::map(const double *pos, const double t, sea_loc_t &loc) const
{
	double tc, tl = (t - min.t) / step.t;
	if ((tl < 0.0) || (tl > max_loc.t))
		return false;
	loc.t.frac = modf(tl, &tc);
	loc.t.cell = tc;

	if (pos[Z] >= depth_data[0])
	{
		loc.z.cell = 0;
		loc.z.frac = 0.0;
	}
	else if (pos[Z] <= depth_data[SEA_NZ - 1])
	{
		loc.z.cell = SEA_NZ - 2;
		loc.z.frac = 0.999999;
	}
	else
	{
		unsigned int zi;
		for (zi = SEA_NZ - 2; zi > 0; --zi)
			if (pos[Z] <= depth_data[zi])
				break;
		loc.z.cell = zi;
		loc.z.frac = (pos[Z] - depth_data[zi]) / (depth_data[zi + 1] - depth_data[zi]);
	}

	double yc, y = (pos[Y] - min.y) / step.y;
	if ((y < 0.0) || (y > max_loc.y))
		return false;
	loc.y.frac = modf(y, &yc);
	loc.y.cell = yc;

	double xc, x = (pos[X] - min.x) / step.x;
	if ((x < 0.0) || (x > max_loc.x))
		return false;
	loc.x.frac = modf(x, &xc);
	loc.x.cell = xc;

	// printf("MAP\n");
	// printf("\tX %f > %u/%f (min %f, step %f, cmax %f)\n", pos[X], loc.x.cell, loc.x.frac, min.x, step.x, max_loc.x);
	// printf("\tY %f > %u/%f (min %f, step %f, cmax %f)\n", pos[Y], loc.y.cell, loc.y.frac, min.y, step.y, max_loc.y);
	// printf("\tZ %f > %u/%f\n",                            pos[Z], loc.z.cell, loc.z.frac);
	// printf("\tT %f > %u/%f (min %f, step %f, cmax %f)\n", t, loc.t.cell, loc.t.frac, min.t, step.t, max_loc.t);

	return true;
}

//-----------------------------------------------------------------------------
double Sea::value(const sea_loc_t loc, const sea_data_t &data) const
{
	double d[8]; // buffer for intermediate values

	// t
	for (unsigned int i = 0; i < 8; ++i)
	{
		const unsigned int
			t = loc.t.cell,
			z = loc.z.cell + (i >> 2),		 // 00001111
			y = loc.y.cell + ((i & 2) >> 1), // 00110011
			x = loc.x.cell + (i & 1);		 // 01010101
		d[i] = interpolate(loc.t.frac, data[t][z][y][x], data[t + 1][z][y][x]);
	}

	// x
	for (unsigned int i = 0; i < 8; i += 2)
	{
		if (!d[i])
			d[i] = d[i + 1];
		else if (d[i + 1])
			d[i] = interpolate(loc.x.frac, d[i], d[i + 1]);
	}

	// y
	if (!d[0])
		d[0] = d[2];
	else if (d[2])
		d[0] = interpolate(loc.y.frac, d[0], d[2]);

	if (!d[4])
		d[4] = d[6];
	else if (d[6])
		d[4] = interpolate(loc.y.frac, d[4], d[6]);

	// z
	if (!d[0])
		d[0] = d[4];
	else if (d[4])
		d[0] = interpolate(loc.z.frac, d[0], d[4]);

	return d[0];
}

//-----------------------------------------------------------------------------
bool Sea::variables(const double *pos, const double t, double *salinity, double *temperature, double *drift) const
{
	sea_loc_t loc;
	if (!map(pos, t, loc))
		return false;

	if (salinity != NULL)
		*salinity = value(loc, salt_data);
	if (temperature != NULL)
		*temperature = value(loc, temp_data);
	if (drift != NULL)
	{
		drift[X] = value(loc, flow_data[X]);
		drift[Y] = value(loc, flow_data[Y]);
		drift[Z] = value(loc, flow_data[Z]) * -1;
	}

	return true;
}

bool Sea::driftvals(const double *pos, const double t, double *drift) const
{
	sea_loc_t loc;
	
	if (!map(pos, t, loc))
		return false;
	
	if (drift != NULL)
	{
		drift[X] = value(loc, flow_data[X]);
		drift[Y] = value(loc, flow_data[Y]);
		drift[Z] = value(loc, flow_data[Z]) * -1;
	}

	return true;
}

//-----------------------------------------------------------------------------
double Sea::soundspeed(const sea_loc_t loc, double z) const
{
	const double salt = value(loc, salt_data);
	if (salt == 0.0)
		return -1.0;
	const double temp = value(loc, temp_data);
	if (temp == 0.0)
		return -1.0;

	return soundspeed_from_STD(salt, temp, pressure_from_depth(z));
}

double Sea::soundspeed(const double *pos, const double t) const
{
	sea_loc_t loc;
	if (!map(pos, t, loc))
		return -1.0;

	return soundspeed(loc, pos[Z]);
}

//-----------------------------------------------------------------------------
double Sea::min_soundspeed(const double *pos, const double t, double max_depth, double *min_soundspeed_depth) const
{
	sea_loc_t loc;
	if (!map(pos, t, loc))
		return -1.0;

	loc.z.cell = SEA_NZ - 2;

	loc.z.frac = 0.999999;
	double ss_min_z = depth_data[SEA_NZ - 1];
	double ss_min = soundspeed(loc, ss_min_z);

	loc.z.frac = 0.0;
	for (double z = depth_data[loc.z.cell]; z < max_depth; z = depth_data[--loc.z.cell])
	{
		const double ss = soundspeed(loc, z);
		if (ss <= 0.0)
			break;
		if (ss < ss_min)
		{
			ss_min = ss;
			ss_min_z = z;
		}
		if (loc.z.cell == 0)
			break;
	}

	if (min_soundspeed_depth != NULL)
		*min_soundspeed_depth = ss_min_z;
	return ss_min;
}
