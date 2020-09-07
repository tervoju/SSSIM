/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <stdarg.h>
#include <cstdio>
#include <cstdlib>

#include "vt100.h"
#include "sssim.h"
#include "float-structs.h"
#include "client-time.h"

extern SimTime sim_time;


FILE * log_file = NULL;

uint32_t dprint_stdout_timestamp() {
	static uint32_t prev_time = 0;

	uint32_t time = sim_time.now();
	if (time == prev_time) {
		for (int i = 0; i < 12; ++i) putc(' ', stdout);
	} else {
		char * timestr;
		if ((timestr = time2str(time))) {
			fprintf(stdout, VT_SET(VT_BLUE) "%s " VT_RESET, timestr);
			free(timestr);
		}
		prev_time = time;
	}
	return time;
}

void dprint_timestamp(bool log_only) {
	uint32_t t_now = log_only
		? sim_time.now()
		: dprint_stdout_timestamp();
	if (log_file != NULL) fprintf(log_file, "%u\t", t_now);
}

int dprint_printf(const char * fmt, ...) {
	char buffer[1024];

	va_list arg;
	va_start(arg, fmt);
		int len = vsnprintf(buffer, 1024, fmt, arg);
	va_end(arg);

	if (log_file != NULL) {
		fwrite(buffer, 1, len, log_file);
		fflush(log_file);
	}

	return fwrite(buffer, 1, len, stdout);
}
