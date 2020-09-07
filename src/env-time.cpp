/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "env-time.h"

extern simtime_t env_time;


//-----------------------------------------------------------------------------
time_msg_t::time_msg_t(char msg_type):
	buf(NULL),
	len(1 + sizeof(env_time))
{
	buf = new char[len];
	buf[0] = msg_type;
	memcpy(buf + 1, &env_time, sizeof(env_time));
}


//-----------------------------------------------------------------------------
void print_timestr() {
	static simtime_t prev_time = 0;

	if (env_time == prev_time) for (int i = 0; i < 12; ++i) putchar(' ');
	else {
		char * timestr = time2str(env_time);
		if (!timestr) return;
		printf("\e[34m%s\e[0m ", timestr);
		free(timestr);
		prev_time = env_time;
	}
}


//-----------------------------------------------------------------------------
simtime_t time_init(int argc, char **argv) {
	srand48(time(NULL));

	// command line arguments
	if ((argc > 1) && ((argv[1][0] == '?') || (argv[1][1] == '?'))) {
		printf("usage: %s [start time]\n", argv[0]);
		printf("\tdefault start time is random\n");
		exit(EXIT_SUCCESS);
	}

	long t0 = (argc < 2) ? -1 : atol(argv[1]);
	if (t0 < 0) t0 = 24 * 3600 * 25.0 * drand48();

	return t0;
}
