/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "udp.h"
#include "sssim-structs.h"
#include "float-structs.h"
#include "client-socket.h"
#include "util-math.h"
#include "client-time.h"
#include "float-util.h"
#include "sat-client.h"

extern sockaddr_in env_addr;
extern SimTime sim_time;

//-----------------------------------------------------------------------------
SatClient::SatClient(simtime_t _refresh_time, simtime_t _connection_mean_time) : thread_id(),
																				 refresh_time(_refresh_time),
																				 connection_mean_time(_connection_mean_time),
																				 skt(&env_addr),
																				 run_mutex(), run_cond(),
																				 connect_mutex(), connect_cond(),
																				 enabled(false), connected(false)
{
	pthread_mutex_init(&run_mutex, NULL);
	pthread_cond_init(&run_cond, NULL);
	pthread_mutex_init(&connect_mutex, NULL);

	pthread_create(&thread_id, NULL, &thread_wrap, this);
}

SatClient::~SatClient()
{
	pthread_mutex_destroy(&run_mutex);
	pthread_cond_destroy(&run_cond);
	pthread_mutex_destroy(&connect_mutex);
}

//-----------------------------------------------------------------------------
void *SatClient::thread()
{
	while (true)
	{
		pthread_mutex_lock(&run_mutex);
		if (!enabled)
		{
			sim_time.suspend_sleep();
			pthread_cond_wait(&run_cond, &run_mutex);
		}
		pthread_mutex_unlock(&run_mutex);

		if (!connected)
		{
			while (enabled && !at_surface(&skt))
				sim_time.sleep(refresh_time);
			if (!enabled)
				continue;

			const double ct = connection_mean_time * exponential_rand();
			sim_time.sleep(ct);
		}

		while (enabled)
		{
			update();
			if (enabled) {
				sim_time.sleep(refresh_time);
				//printf("------------------> gps");
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
bool SatClient::start()
{
	pthread_mutex_lock(&run_mutex);
	const bool was_enabled = enabled;
	enabled = true;
	if (!was_enabled)
		connected = false;
	pthread_cond_broadcast(&run_cond);
	pthread_mutex_unlock(&run_mutex);

	return was_enabled;
}

bool SatClient::stop()
{
	pthread_mutex_lock(&run_mutex);
	const bool was_enabled = enabled;
	enabled = false;
	connected = false;
	pthread_mutex_unlock(&run_mutex);

	return was_enabled;
}

//-----------------------------------------------------------------------------
bool SatClient::wait_for_connection(long timeout_ns)
{
	if (!enabled)
		return false;

	int rv;
	pthread_mutex_lock(&connect_mutex);
	if (connected)
	{
		rv = 0;
	}
	else
	{
		sim_time.suspend_sleep();
		if (timeout_ns > 0)
		{
			const timespec ts = {timeout_ns / 1e9l, timeout_ns};
			rv = pthread_cond_timedwait(&connect_cond, &connect_mutex, &ts);
		}
		else
		{
			rv = pthread_cond_wait(&connect_cond, &connect_mutex);
		}
		sim_time.enable_sleep();
	}
	pthread_mutex_unlock(&connect_mutex);

	return (rv == 0);
}
