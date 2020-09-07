/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <queue>
#include <map>
#include <algorithm>
#include <netinet/in.h>
#include <svl/SVL.h>

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
#include "client-init.h"
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

extern int16_t client_id;
extern SimTime sim_time;

//-----------------------------------------------------------------------------
groupfloat::groupfloat() : depthcodes(),
						   gps_pos()
{
	memset(depthcodes, DEPTHCODE_UNKNOWN, max_cycle_count);
}

//-----------------------------------------------------------------------------
depthcode groupfloat::get_depthcode(int c)
{
	return ((c >= 0) && (c < max_cycle_count)) ? depthcodes[c] : depthcode(DEPTHCODE_UNKNOWN, true, true);
}

//-----------------------------------------------------------------------------
void groupfloat::set_depthcode(int c, depthcode d)
{
	if ((c >= 0) && (c < max_cycle_count))
		depthcodes[c] = d;
}

//-----------------------------------------------------------------------------
void groupfloat::add_gps_measurement(simtime_t time, double x, double y, Seafloat *seafloat)
{
	const pos_xyt_conf m(x, y, time, 1.0);
	gps_pos = m;
}

void groupfloat::add_gps_estimate(simtime_t time, double x, double y, double z, Seafloat *seafloat)
{
	const pos_xyzt m(x, y, z, time, 1.0);
	gps_est = m;
}