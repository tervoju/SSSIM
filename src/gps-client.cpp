/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "udp.h"
#include "client-socket.h"
#include "sssim-structs.h"
#include "float-util.h"
#include "sat-client.h"
#include "gps-client.h"

extern sockaddr_in env_addr;

//-----------------------------------------------------------------------------
GPSclient::GPSclient(simtime_t _refresh_time, simtime_t _connection_mean_time) : SatClient(_refresh_time, _connection_mean_time),
																				 data()
{
	data.ok = false;
}

//-----------------------------------------------------------------------------
void GPSclient::update()
{
	const gps_data_t new_data = get_gps(&skt);

	pthread_mutex_lock(&run_mutex);
	pthread_mutex_lock(&connect_mutex);
	connected = (enabled && new_data.ok); // ok is false if below surface
	if (connected)
	{
		data = new_data;
		pthread_cond_broadcast(&connect_cond);
	}
	pthread_mutex_unlock(&connect_mutex);
	pthread_mutex_unlock(&run_mutex);
}

//-----------------------------------------------------------------------------
bool GPSclient::get_position(gps_data_t *gps_pos, bool ignore_state)
{
	if (!ignore_state && !enabled)
		return false;

	pthread_mutex_lock(&connect_mutex);
	memcpy(gps_pos, &data, sizeof(gps_data_t));
	bool ok = connected;
	pthread_mutex_unlock(&connect_mutex);

	return ok;
}
