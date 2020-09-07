/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <vector>
#include <netinet/in.h>
#include <svl/SVL.h>
#include <deque>

#include "util-leastsquares.h"
#include "util-convert.h"
#include "util-math.h"
#include "udp.h"
#include "client-socket.h"
#include "client-print.h"
#include "sssim.h"
#include "sssim-structs.h"
#include "ctd-tracker.h"
#include "ctd-estimate.h"

extern sockaddr_in env_addr;

//-----------------------------------------------------------------------------
bool at_surface(SimSocket *env_skt)
{
	//FIXME: using a whole FloatState is probably too much
	FloatState fs;
	ssize_t rx_bytes = env_skt->get_data(&fs, sizeof(fs), MSG_GET_INFO);
	if (rx_bytes != sizeof(fs))
		return false;

	return (fs.pos[3] < DRIFTER_SURFACE_DEPTH);
}
bool at_surface()
{
	SimSocket skt(&env_addr);
	return at_surface(&skt);
}

//-----------------------------------------------------------------------------
unsigned char get_echo(SimSocket *env_skt)
{
	unsigned char echo;
	ssize_t rx_bytes = env_skt->get_data(&echo, sizeof(echo), MSG_GET_ECHO);
	if (rx_bytes != sizeof(echo))
	{
		if (rx_bytes == 0)
			dprint("%s: rx timeout\n", __FUNCTION__);
		else
		{
			dprint("%s: rx %ib != %ib:", __FUNCTION__, rx_bytes, sizeof(echo));
			for (ssize_t i = 0; i < rx_bytes; ++i)
				printf(" %02X", reinterpret_cast<char *>(&echo)[i]);
			putchar('\n');
		}
		return ALTIM_SILENCE;
	}

	return echo;
}
unsigned char get_echo()
{
	SimSocket skt(&env_addr);
	return get_echo(&skt);
}

//-----------------------------------------------------------------------------
ctd_data_t get_ctd(SimSocket *env_skt)
{
	ctd_data_t ctd;
	ssize_t rx_bytes = env_skt->get_data(&ctd, sizeof(ctd), MSG_GET_CTD);
	if (rx_bytes != sizeof(ctd))
	{
		if (rx_bytes == 0)
			dprint("%s: rx timeout\n", __FUNCTION__);
		else
		{
			dprint("%s: rx %ib != %ib:", __FUNCTION__, rx_bytes, sizeof(ctd));
			for (ssize_t i = 0; i < rx_bytes; ++i)
				printf(" %02X", reinterpret_cast<char *>(&ctd)[i]);
			putchar('\n');
		}
		return ctd_data_t();
	}

	ctd.pressure += 3.0 * norm_rand();

	return ctd;
}
ctd_data_t get_ctd()
{
	SimSocket skt(&env_addr);
	return get_ctd(&skt);
}

//-----------------------------------------------------------------------------
gps_data_t get_gps(SimSocket *env_skt)
{
	gps_data_t gps;
	ssize_t rx_bytes = env_skt->get_data(&gps, sizeof(gps), MSG_GET_GPS);
	if (rx_bytes != sizeof(gps))
		gps.ok = false;

	return gps;
}
gps_data_t get_gps()
{
	SimSocket skt(&env_addr);
	return get_gps(&skt);
}

//-----------------------------------------------------------------------------
double get_float_density(SimSocket *env_skt)
{
	double density;
	ssize_t rx_bytes = env_skt->get_data(&density, sizeof(density), MSG_GET_DENSITY);
	if (rx_bytes != sizeof(density))
	{
		if (rx_bytes == 0)
		{
			dprint("%s: rx timeout\n", __FUNCTION__);
			return -1.0;
		}
		else if (rx_bytes > 0)
		{
			dprint("%s: rx %ib != %ib:", __FUNCTION__, rx_bytes, sizeof(density));
			for (ssize_t i = 0; i < rx_bytes; ++i)
				printf(" %02X", reinterpret_cast<char *>(&density)[i]);
			putchar('\n');
			return -2.0;
		}
		return -3.0;
	}

	return density;
}
double get_float_density()
{
	SimSocket skt(&env_addr);
	return get_float_density(&skt);
}

//-----------------------------------------------------------------------------
bool set_float_density(double new_density, SimSocket *env_skt)
{
	bool ok;
	ssize_t rx_bytes = env_skt->get_data(&ok, sizeof(ok), MSG_SET_DENSITY, &new_density, sizeof(new_density));
	if (rx_bytes != sizeof(ok))
		return false;

	return ok;
}
bool set_float_density(double new_density)
{
	SimSocket skt(&env_addr);
	return set_float_density(new_density, &skt);
}

//-----------------------------------------------------------------------------
double approx_baltic_density(double depth)
{
	static double
		top[] = {-0.01458064, 1003.65416396},
		bot[] = {5.82791451e-13, 2.93618412e-10, 5.80359192e-08, 5.74515221e-06, 3.01506402e-04, 7.94346722e-03, 3.38527732e-02, 1003.71941686};
	double
		r = 0.0,
		*pa;
	unsigned int i, pa_size;

	if (depth < 6)
	{
		pa = &top[0];
		pa_size = sizeof(top) / sizeof(double);
	}
	else
	{
		pa = &bot[0];
		pa_size = sizeof(bot) / sizeof(double);
	}

	for (i = 0; i < pa_size; ++i)
		r = pa[i] - depth * r;

	return r;
}

//-----------------------------------------------------------------------------
bool print_status()
{
	FloatState fs;
	ssize_t rx_bytes = SimSocket(&env_addr).get_data(&fs, sizeof(fs), MSG_GET_INFO);
	if (rx_bytes != sizeof(fs))
		return false;

	printf("pos[ %.0f, %.0f, %.0f ], vel[ %.1f, %.1f ]", fs.pos[0], fs.pos[1], fs.pos[2], 100 * fs.vel[0], 100 * fs.vel[1]);
	return true;
}

//-----------------------------------------------------------------------------
T_EnvData get_envstate(uint32_t depth = 0)
{
	T_EnvData ed;
	ssize_t rx_bytes = (depth > 0)
						   ? SimSocket(&env_addr).get_data(&ed, sizeof(ed), MSG_GET_ENV, &depth, sizeof(depth))
						   : SimSocket(&env_addr).get_data(&ed, sizeof(ed), MSG_GET_ENV);

	if (rx_bytes != sizeof(ed))
		ed.depth = -1.0;
	return ed;
}

bool print_envstate()
{
	T_EnvData ed = get_envstate();

	printf("%.1fm: %.2f°C, %.2fS, flow [ %.1f, %.1f ]", ed.depth, ed.temperature, ed.salinity, ed.drift_x, ed.drift_y);
	return ed.depth >= 0.0;
}

//-----------------------------------------------------------------------------
bool compare_profile()
{
	static uint32_t test_depths[] = {2, 10, 30, 60, 90};
	static int test_length = sizeof(test_depths) / sizeof(uint32_t);

	SimSocket env_skt(&env_addr);

	for (int i = 0; i < test_length; ++i)
	{
		T_EnvData ed = get_envstate(test_depths[i]);
		if (ed.depth < 0.0)
			break;

		unsigned int pressure = pressure_from_depth(ed.depth);
		double dm = density_from_STD(ed.salinity, ed.temperature, pressure);
		double se = estimate_salinity(ed.depth);
		double te = estimate_temperature(ed.depth);
		double de = density_from_STD(se, te, pressure);

		double v;
		dprint("~\t%.1f", ed.depth);
		putchar('\t');
		v = ed.temperature - te;
		if (fabs(v) < 20.0)
			printf("%.4f", v);
		putchar('\t');
		v = ed.salinity - se;
		if (fabs(v) < 20.0)
			printf("%.4f", v);
		putchar('\t');
		v = dm - de;
		if (fabs(v) < 20.0)
			printf("%.4f", v);
		//putchar('\t');                          if (z!=0.0) printf("%.4f", ed.depth - z);
		putchar('\n');
	}

	return true;
}
