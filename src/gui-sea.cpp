/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include <cstdio>
#include <stdint.h>
#include <netcdfcpp.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "sea-data.h"
#include "gui-sea.h"


//-----------------------------------------------------------------------------
void GuiSea::VerifyNetCDF( NcFile * pf, unsigned int level, std::string s ) {
	int bad = 0;
	if ( !pf->is_valid() ) bad = 1;
	if ( !bad && ( level >= 1 ) && ( ( pf->get_dim(SEA_XVAR)->size() != SEA_NX_MAX ) || ( pf->get_dim(SEA_YVAR)->size() != SEA_NY_MAX ) ) ) bad = 2;
	if ( !bad && ( level >= 2 ) && ( pf->get_dim("T")->size() != (int)SEA_NT ) ) bad = 3;
	if ( !bad && ( level >= 3 ) && ( pf->get_dim("Z")->size() != (int)SEA_NZ ) ) bad = 4;

	if ( bad ) {
		std::cout << s << " data BAD(" << bad << ") :: exiting\n";
		exit(bad);
	}
}

void GuiSea::ReadDepthInfo() {
	VerifyNetCDF(pfDepth, 0, "Depth");

	double var[2];
	int bottom_x0, bottom_y0, bottom_nx, bottom_ny;

	pfDepth->get_var("IOWLON")->set_cur(0, -1);
	pfDepth->get_var("IOWLON")->get(var, 2);
	var[1] -= var[0];
	bottom_x0 = static_cast<int>((var_pos[0][0] - var[0]) / var[1] - 0.1);
	bottom_nx = static_cast<int>((var_pos[0][0] + (SEA_NX-1) * var_pos[0][1] - var[0]) / var[1] + 2.1) - bottom_x0;
	//std::cout << "#define  SEA_DEPTH_OFFSET_X  " << bottom_x0 << '\n';
	//std::cout << "#define  SEA_DEPTH_NX  " << bottom_nx << '\n';

	pfDepth->get_var("IOWLAT2")->set_cur(0, -1);
	pfDepth->get_var("IOWLAT2")->get(var, 2);
	var[1] -= var[0];
	bottom_y0 = static_cast<int>((var_pos[1][0] - var[0]) / var[1] - 0.1);
	bottom_ny = static_cast<int>((var_pos[1][0] + (SEA_NY-1) * var_pos[1][1] - var[0]) / var[1] + 2.1) - bottom_y0;
	//std::cout << "#define  SEA_DEPTH_OFFSET_Y  " << bottom_y0 << '\n';
	//std::cout << "#define  SEA_DEPTH_NY  " << bottom_ny << '\n';
	if ((bottom_nx != SEA_DEPTH_NX) || (bottom_ny != SEA_DEPTH_NY)) {
		printf("Sea bottom area changed. Please recompile with the following in src/*-sea.h:\n\n");
		printf("\t#define  SEA_DEPTH_NX  %i\n", bottom_nx);
		printf("\t#define  SEA_DEPTH_NY  %i\n\n", bottom_ny);
		exit(1);
	}

	dDepth->set_cur(bottom_y0, bottom_x0);
	dDepth->get(&bottom[0][0], bottom_ny, bottom_nx);

	pfDepth->get_var("IOWLAT2")->set_cur(bottom_y0, -1);
	pfDepth->get_var("IOWLAT2")->get(&bottom_pos[1][0], 2);
	pfDepth->get_var("IOWLON")->set_cur(bottom_x0, -1);
	pfDepth->get_var("IOWLON")->get(&bottom_pos[0][0], 2);

	bottom_pos[0][1] -= bottom_pos[0][0];
	bottom_pos[1][1] -= bottom_pos[1][0];

	bottom_pos[0][0] = degrees_east_to_meters(bottom_pos[0][0]);
	bottom_pos[1][0] = degrees_north_to_meters(bottom_pos[1][0]);
	bottom_pos[0][1] *= METERS_PER_DEGREE_EAST;
	bottom_pos[1][1] *= METERS_PER_DEGREE_NORTH;

	//std::cout << "\nbottom x " << bottom_pos[0][0] << " :: " << bottom_pos[0][1] << ", y " << bottom_pos[1][0] << " :: " << bottom_pos[1][1] << '\n';
}


//-----------------------------------------------------------------------------
GuiSea::GuiSea():
	t_now(0.0),
	
	//Read the data from the data files, and use respective file pointers to point to them
	pfDepth (new NcFile(SEA_DATA_FILEPATH_DEPTH, NcFile::ReadOnly)),
	pfS     (new NcFile(SEA_DATA_DIR SEA_DATA_FILE_SALT, NcFile::ReadOnly)),
	pfTemp  (new NcFile(SEA_DATA_DIR SEA_DATA_FILE_TEMP, NcFile::ReadOnly)),
	pfU     (new NcFile(SEA_DATA_DIR SEA_DATA_FILE_U_VEL, NcFile::ReadOnly)),
	pfV     (new NcFile(SEA_DATA_DIR SEA_DATA_FILE_V_VEL, NcFile::ReadOnly)),
	pfW     (new NcFile(SEA_DATA_DIR SEA_DATA_FILE_W_VEL, NcFile::ReadOnly)),

	//Use the variable pointer to point to the respective variables:
		// dDepth  -> depth
		// dS      -> salinity
		// dTemp   -> temperature
		// dU      ->
		// dV      ->
		// dW      ->

	dDepth (pfDepth->get_var("BALDEP")),
	dS (pfS->get_var("S")),
	dTemp (pfTemp->get_var("TEMP")),
	dU (pfU->get_var("U")),
	dV (pfV->get_var("V")),
	dW (pfW->get_var("W")),

	t0 (0), time_stepsize (1), t_now_int(0), t_now_frac(0.0)  // Initialize the time constants
{
	VerifyNetCDF(pfS, 3, "S");
	VerifyNetCDF(pfTemp, 3, "Temp");
	VerifyNetCDF(pfU, 3, "U");
	VerifyNetCDF(pfV, 3, "V");
	VerifyNetCDF(pfW, 3, "W");

	NcFile * pf = pfTemp;	// use data from Temp variable for global settings -- assumed same for all variables

	pf->get_var(SEA_YVAR)->set_cur(SEA_IY, -1);
	pf->get_var(SEA_YVAR)->get(&var_pos[1][0], 2);
	pf->get_var(SEA_XVAR)->set_cur(SEA_IX, -1);
	pf->get_var(SEA_XVAR)->get(&var_pos[0][0], 2);
	var_pos[0][1] -= var_pos[0][0];
	var_pos[1][1] -= var_pos[1][0];
	var_pos[1][0] -= var_pos[1][1]/2; // DEBUG to better match bathymetry

	ReadDepthInfo();

	var_pos[0][0] = degrees_east_to_meters(var_pos[0][0]);
	var_pos[1][0] = degrees_north_to_meters(var_pos[1][0]);
	var_pos[0][1] *= METERS_PER_DEGREE_EAST;
	var_pos[1][1] *= METERS_PER_DEGREE_NORTH;

	pf->get_var("Z")->get( &depth[0], SEA_NZ );
	for (unsigned int z = 0; z < SEA_NZ; ++z) depth[z] *= -1;

	int at[2];
	pf->get_var("T")->get(at, 2);
	t0 = at[0];
	time_stepsize = at[1] - at[0];
	t_now_int = SEA_NT * time_stepsize + 1;

	SetTime(0, true);
}


//-----------------------------------------------------------------------------
void GuiSea::UpdateDataToTime( SeaData & tgt, SeaData * raw ) {
	for ( unsigned int z = 0; z < SEA_NZ; ++z ) for ( unsigned int y = 0; y < SEA_NY; ++y ) for ( unsigned int x = 0; x < SEA_NX; ++x ) {
		tgt[z][y][x] = (1.0 - t_now_frac) * raw[0][z][y][x] + t_now_frac * raw[1][z][y][x];
	}
}


//-----------------------------------------------------------------------------
void GuiSea::SetTime( double t, bool update_grid ) {
	static NcVar * v[] = { dS, dTemp, dU, dV, dW };
	static SeaData * dr[] = { &raw_salt[0], &raw_temp[0], &raw_flow[0][0], &raw_flow[1][0], &raw_flow[2][0] };
	static SeaData * di[] = { &salt, &temp, &flow[0], &flow[1], &flow[2] };

	double ti_d;
	double tf = modf( t / time_stepsize, &ti_d );
	uint32_t ti = static_cast<uint32_t>( ti_d );

	if ( ti >= SEA_NT-1 ) {
		t = 0.0;
		ti = 0;
		tf = 0.0;
	}

	if ( ti != t_now_int ) {
		for ( unsigned int i = 0; i < 5; ++i ) {
			v[i]->set_cur( ti, 0, SEA_IY, SEA_IX );
			v[i]->get( &dr[i][0][0][0][0], 2, SEA_NZ, SEA_NY, SEA_NX );
		}
	}

	t_now = t;
	t_now_int = ti;
	t_now_frac = tf;

	if (update_grid) for (unsigned int i = 0; i < 5; ++i) UpdateDataToTime(*(di[i]), dr[i]);
}


//-----------------------------------------------------------------------------
void GuiSea::PrintInfo() {
	NcFile * pf = pfTemp;	// use data from Temp variable for global settings -- assumed same for all variables

	unsigned int
		_nx = pf->get_dim(SEA_XVAR)->size(),
		_ny = pf->get_dim(SEA_YVAR)->size(),
		_nz = pf->get_dim("Z")->size(),
		_nt = pf->get_dim("T")->size();

	putchar('\n');
	printf("#define  SEA_NX_MAX  %u\n", _nx);
	printf("#define  SEA_NY_MAX  %u\n", _ny);
	printf("#define  SEA_NZ  %u\n", _nz);
	printf("#define  SEA_NT  %u\n", _nt);

	printf("\n// data ranges:\n");
	printf("//   x (lon) start %.2f E, increment %.2f, end %.2f E\n",
		meters_east_to_degrees(var_pos[0][0]), var_pos[0][1] / METERS_PER_DEGREE_EAST,
		meters_east_to_degrees(var_pos[0][0] + (SEA_NX - 1) * var_pos[0][1]));
	printf("//   y (lat) start %.2f N, increment %.2f, end %.2f N\n",
		meters_north_to_degrees(var_pos[1][0]), var_pos[1][1] / METERS_PER_DEGREE_NORTH,
		meters_north_to_degrees(var_pos[1][0] + (SEA_NY - 1) * var_pos[1][1]));
	printf("//   z layers, in meters:\n//     %.2f", depth[0]);
	for (unsigned int z = 1; z < _nz; ++z) printf(", %.2f", depth[z]);
	putchar('\n');

	double range[2], a[_nz][_ny][_nx];
	NcVar * v[] = { dS, dTemp, dU, dV, dW };
	std::string n[] = { "SALT", "TEMP", "FLOW_U", "FLOW_V", "FLOW_W" };

	for ( unsigned int i = 0; i < 5; ++i ) {
		printf("\n// %s range", v[i]->get_att("long_name")->as_string(0)); fflush(stdout);
		range[0] = std::numeric_limits<double>::max();
		range[1] = -1 * std::numeric_limits<double>::max();

		for (uint32_t t = 0; t < SEA_NT; ++t) {
			v[i]->set_cur( t, 0, 0, 0 );
			v[i]->get(&a[0][0][0], 1, _nz, _ny, _nx);
			for ( unsigned int z = 0; z < _nz; ++z )
				for ( unsigned int y = 0; y < _ny; ++y )
					for ( unsigned int x = 0; x < _nx; ++x ) {
						double & d = a[z][y][x];
						if ( d && ( fabs(d) > std::numeric_limits<double>::epsilon() ) ) {
							if ( d < range[0] ) range[0] = d;
							if ( d > range[1] ) range[1] = d;
						}
					}
		}
		if (v[i] != dS) printf(" (%s)", v[i]->get_att("units")->as_string(0));
		printf("\n#define  SEA_%s_MIN  %f\n", n[i].c_str(), range[0]);
		printf(  "#define  SEA_%s_MAX  %f\n", n[i].c_str(), range[1]);
	}


	std::cout << '\n';
}
