/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <netinet/in.h>

#include "sssim.h"
#include "udp.h"

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: %s start|stop|quit\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char msg;
	if (!strcmp(argv[1], "start"))
		msg = MSG_PLAY;
	else if (!strcmp(argv[1], "stop"))
		msg = MSG_PAUSE;
	else if (!strcmp(argv[1], "quit"))
		msg = MSG_QUIT;
	else
	{
		printf("usage: %s start|stop|quit\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sockaddr_in env_addr = inet(SSS_ENV_ADDRESS).addr;
	ssize_t ok = UDPclient(&env_addr).send(&msg, 1);
	exit((ok > 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}
