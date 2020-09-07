/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <unistd.h>
#include <map>
#include <netinet/in.h>

#include "sssim.h"
#include "udp.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"

extern sockaddr_in env_addr;

//-----------------------------------------------------------------------------
struct sleeper_t
{
	bool may_sleep;
	simtime_t wakeup_time;

	void set(simtime_t t)
	{
		wakeup_time = t;
		may_sleep = true;
	}
};

std::map<pthread_t, sleeper_t> sleepers; // NOTE: assumes pthread_t is a basic type

//-----------------------------------------------------------------------------
SimTime::SimTime() : mutex(),
					 run_cond(),
					 time_now(0),
					 cycle()
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&run_cond, NULL);
}

SimTime::~SimTime()
{
	pthread_cond_destroy(&run_cond);
	pthread_mutex_destroy(&mutex);
}

//-----------------------------------------------------------------------------
int SimTime::set(char ctrl, const void *buf, ssize_t len)
{
	const simtime_t *tbuf = reinterpret_cast<const simtime_t *>(buf);
	const size_t ts = sizeof(simtime_t);

	bool ok = false;
	pthread_mutex_lock(&mutex);
	switch (len)
	{
	case ts:
		time_now = *tbuf; // fallthrough
	case 0:
		ok = (ctrl == MSG_TIME);
	}
	//dprint_timestamp(); putchar('\n');
	if (ok)
		pthread_cond_broadcast(&run_cond);
	pthread_mutex_unlock(&mutex);

	return ok ? 0 : -1;
}

//-----------------------------------------------------------------------------
int SimTime::cycle_number(simtime_t t) const
{
	if (!cycle.t_start)
		return -3;
	if (!cycle.t_period)
		return -2;
	if (!t)
		t = now();
	if (t < cycle.t_start)
		return -1;
	return (t - cycle.t_start) / cycle.t_period;
}

int SimTime::cycle_time(simtime_t t, bool pre_start_ok) const
{
	if (!cycle.t_start)
		return -3;
	if (!cycle.t_period)
		return -2;
	if (!t)
		t = now();
	if (t < cycle.t_start)
	{
		if (pre_start_ok)
		{
			simtime_t t0 = cycle.t_start - cycle.t_period;
			while (t < t0)
				t0 -= cycle.t_period;
			return t - t0;
		}
		else
			return -1;
	}
	return (t - cycle.t_start) % cycle.t_period;
}

//-----------------------------------------------------------------------------
void SimTime::do_sleep_until(const simtime_t t) const
{
	static SimSocket skt(&env_addr);
	skt.send(MSG_SLEEP, &t, sizeof(t));
	//dprint("%lu: sleeping %is\n", pthread_self(), (int)t - time_now);
}

simtime_t SimTime::sleep_until(const simtime_t wakeup_time)
{
	pthread_mutex_lock(&mutex);
	sleepers[pthread_self()].set(wakeup_time);

	if (wakeup_time <= time_now)
	{
		pthread_mutex_unlock(&mutex);
		return time_now;
	}

	simtime_t tw = wakeup_time;
	FOREACH_CONST(it, sleepers)
	{
		const sleeper_t &s = it->second;
		if (s.may_sleep && (s.wakeup_time < tw))
			tw = s.wakeup_time;
		//dprint("sleeper %lu: %s %is\n", it->first, s.may_sleep ? "ON" : "OFF", (int)s.wakeup_time - time_now);
	}
	//dprint("     >> %lu: %is\n", pthread_self(), (int)tw - time_now);
	if (tw > time_now)
		do_sleep_until(tw);

	while (time_now < wakeup_time)
	{
		//dprint("        %lu: sleeping %is...\n", pthread_self(), (int)wakeup_time - time_now);
		pthread_cond_wait(&run_cond, &mutex);
	}
	//dprint("        %lu: sleep done!\n", pthread_self());
	pthread_mutex_unlock(&mutex);
	return time_now;
}

simtime_t SimTime::sleep(int sleep_seconds) { return sleep_until(time_now + sleep_seconds); }

void SimTime::enable_sleep()
{
	pthread_mutex_lock(&mutex);
	sleepers[pthread_self()].set(time_now);
	pthread_mutex_unlock(&mutex);

	//dprint("%lu: sleep enabled\n", pthread_self());
}

void SimTime::suspend_sleep()
{
	pthread_mutex_lock(&mutex);
	sleepers[pthread_self()].may_sleep = false;

	bool no_sleep = true;
	simtime_t tw = 0;
	FOREACH_CONST(it, sleepers)
	{
		const sleeper_t &s = it->second;
		if (s.may_sleep && (no_sleep || (s.wakeup_time < tw)))
		{
			tw = s.wakeup_time;
			no_sleep = false;
		}
		//dprint("sleeper %lu: %s %is\n", it->first, s.may_sleep ? "ON" : "OFF", (int)s.wakeup_time - time_now);
	}
	//dprint("     >> %lu: %is\n", pthread_self(), (int)tw - time_now);

	if (tw > time_now)
		do_sleep_until(tw);
	pthread_mutex_unlock(&mutex);

	//dprint("%lu: sleep suspended\n", pthread_self());
}
