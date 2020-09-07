/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <list>
#include <vector>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netcdfcpp.h>
#include <svl/SVL.h>

// added
#include <unistd.h>

#include "util-trigger.h"
#include "util-mkdirp.h"
#include "sssim.h"
#include "sssim-structs.h"
#include "util-convert.h"
#include "sea-data.h"
#include "env-sea.h"
#include "env-float.h"
#include "env-server.h"
#include "env-time.h"
#include "env-clients.h"
#include "vt100.h"

const simtime_t gui_sleep_time = 10;

trigger_t runtime(false);
simtime_t env_time = 0;
Sea *sea;

T_env_client_vector clients;
int clients_awake = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_sleeping = PTHREAD_COND_INITIALIZER;

//-----------------------------------------------------------------------------
void exit_on_signal(int sig) { exit(sig); }

class cpufreq_scaler
{
  public:
	static int rc;

	cpufreq_scaler()
	{
		set_performance();
		if (rc == 0)
		{
			atexit(cpufreq_scaler::set_ondemand);
			signal(SIGINT, exit_on_signal);
		}
	}

	static void set(const char *governor)
	{
		if (rc)
			return;

		char cmd[255] = "GOV=            ; for CPU in $(cat /proc/cpuinfo | grep 'core id' | rev | cut -c1); do /usr/bin/cpufreq-selector --cpu $CPU --governor $GOV; done";
		const char *c = governor;
		for (unsigned int i = 4; i < 16; ++i, ++c)
		{
			if ((*c < 'a') || (*c > 'z'))
				break;
			cmd[i] = *c;
		}

		rc = system(cmd);
	}
	static void set_ondemand() { set("ondemand"); }
	static void set_performance() { set("performance"); }
} cpufreq_scaler;

int cpufreq_scaler::rc = 0;

//-----------------------------------------------------------------------------
double *all_float_pos(size_t *n_floats)
{
	pthread_mutex_lock(&clients_mutex);
	*n_floats = clients.size();
	double *pa = new double[2 * (*n_floats)];
	unsigned int i_f = 0;
	FOREACH_CONST(it, clients)
	{
		if (it->simfloat)
		{
			pa[i_f + 0] = it->simfloat->pos[0];
			pa[i_f + 1] = it->simfloat->pos[1];
		}
		i_f += 2;
	}
	pthread_mutex_unlock(&clients_mutex);

	return pa;
	//delete[] pa;
}

//-----------------------------------------------------------------------------
void init_logs(const simtime_t start_time)
{
	const uint32_t rand = lrand48() >> 8;

	char logdir[32];
	sprintf(logdir, LOGDIR_FMT); // NOTE: fmt probably includes start_time

	int r = mkdirp(logdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); // drwxr-xr-x
	if ((r != 0) && (errno != EEXIST))
	{
		perror("init_logs: mkdirp");
		return;
	}

	char prev_tgt[32];
	errno = 0;
	if (readlink(LOGDIR_SYMLINK, prev_tgt, 32) > 0)
	{
		if (unlink(LOGDIR_SYMLINK))
		{
			perror("init_logs: unlink");
			return;
		}
	}
	else if (errno != ENOENT)
	{
		perror("init_logs: readlink");
		return;
	}

	const char *logname = strrchr(logdir, '/') + 1;
	r = symlink(logname, LOGDIR_SYMLINK);
}

//-----------------------------------------------------------------------------
void show_timerate(bool end)
{
	static timeval rt0;
	static const simtime_t env_time_0 = env_time;

	if (!end)
	{
		gettimeofday(&rt0, NULL);

		printf("|| start time %u: %s\n", env_time, time2str(env_time));
	}
	else
	{
		timeval rt1;
		gettimeofday(&rt1, NULL);
		const double rt_total = rt1.tv_sec - rt0.tv_sec + 1.0e-6 * (rt1.tv_usec - rt0.tv_usec);
		const simtime_t env_time_total = env_time - env_time_0;

		printf(VT_ERASE_BELOW "|| simulated %s in %.2f seconds: %.1f × realtime\n", time2str(env_time_total), rt_total, env_time_total / rt_total);
		//getchar();
	}
}

//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	printf("\e]0;Environment\a");
	fflush(stdout);

	//printf("|| Creating environment..."); fflush(stdout);
	//printf("\e[2K\r|| Environment ready\n\n");

	env_time = time_init(argc, argv);
	init_logs(env_time);
	udp_init();

	sea = new Sea();

	show_timerate(false);

	timeval tv0 = {0, 0};
	const simtime_t env_end_time = sea->min.t + SEA_NT * sea->step.t/3; // limiting simulation?
	printf("-----------------> endtime: %d \n", env_end_time);
	while (++env_time < env_end_time)
	{
		if (!(env_time & 0x111))
		{
			timeval tv1;
			gettimeofday(&tv1, NULL);
			if ((tv1.tv_sec != tv0.tv_sec) || (tv1.tv_usec > tv0.tv_usec))
			{
				tv0 = tv1;
				tv0.tv_usec += 1e5; // ~10 Hz
				printf(VT_ERASE_BELOW "\n");
				print_timestr();
				printf("\r" VT_CURSOR_UP);
				fflush(stdout);
			}
		}
		//print_timestr(); putchar('\n');

		runtime.wait_for(true);

		pthread_mutex_lock(&clients_mutex);
		FOREACH(it, clients)
		{
			it->handle_messages();
			if (it->type == BASE)
				continue;
			if (it->simfloat)
				it->simfloat->Update();
			if (it->wakeup_time <= env_time)
			{
				__sync_add_and_fetch(&clients_awake, 1);
				it->wakeup();
			}
			//else printf(VT_SET2(VT_DIM, VT_GREEN) "%u: still sleeping for %is\n" VT_RESET, ntohs(it->addr.sin_port), it->wakeup_time - env_time);
		}

		while (clients_awake > 0)
		{
			//printf(VT_SET(VT_DIM) "clients_awake: %i\n" VT_RESET, clients_awake);
			pthread_cond_wait(&client_sleeping, &clients_mutex);
			//if (clients_awake < 0) printf("clients_awake: %i\n", clients_awake);
		}
		pthread_mutex_unlock(&clients_mutex);
	}

	udp_broadcast(MSG_QUIT);

	printf(VT_ERASE_BELOW "\n|| done.\n");
	show_timerate(true);

	return EXIT_SUCCESS;
}
