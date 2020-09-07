/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <netinet/in.h>
#include <svl/SVL.h>

#include "sssim.h"
#include "sssim-structs.h"
#include "udp.h"
#include "client-socket.h"
#include "client-ctrl.h"
#include "client-print.h"

void *dive_thread_function(void *unused);

extern int16_t client_id;
extern sockaddr_in env_addr;
extern SimSocket *ctrl_socket;
extern FILE *log_file;

//-----------------------------------------------------------------------------
enum ClientType
{
	float_at_sea,
	base_station
};

pthread_t ctrl_thread, dive_thread;

//-----------------------------------------------------------------------------
ssize_t _init_float(int argc, char **argv)
{
	// optional command line parameters, use 0 to skip:
	//   volume pos_x pos_y pos_z vel_x vel_y vel_z
	FloatState fs;
	fs.pos = vl_zero;
	fs.vel = vl_zero;

	switch (argc)
	{
	default:
	case 8:
		fs.vel[2] = atof(argv[7]);
		if (fs.vel[2])
			dprint("init z-vel: %.1lf\n", fs.vel[2]);
	case 7:
		fs.vel[1] = atof(argv[6]);
		fs.vel[0] = atof(argv[5]);
		if (fs.vel[0] || fs.vel[1])
			dprint("init vel: %.2lf, %.2lf\n", fs.vel[0], fs.vel[1]);
	case 6:
	case 5:
		fs.pos[2] = atof(argv[4]);
		if (fs.pos[2])
			dprint("init depth: %.1lf\n", fs.pos[2]);
	case 4:
		fs.pos[1] = atof(argv[3]);
		fs.pos[0] = atof(argv[2]);
		if (fs.pos[0] || fs.pos[1])
			dprint("init pos: %.2lf, %.2lf\n", fs.pos[0], fs.pos[1]);
	case 3:
	case 2:
		fs.volume = atof(argv[1]);
		if (fs.volume)
			dprint("init volume: %.1lf\n", fs.volume);
	case 1:
	case 0:
		break;
	}

	return ctrl_socket->send(MSG_NEW_FLOAT, &fs, sizeof(fs));
}

//-----------------------------------------------------------------------------
void init_client(ClientType type, int argc, char **argv)
{
	printf("%s\n", argv[0] + 2);

	srand48(time(NULL)); // for tri_rand()

	env_addr = inet(SSS_ENV_ADDRESS).addr;
	ctrl_socket = new SimSocket(&env_addr);

	ssize_t tx_bytes = -1;
	switch (type)
	{
	case base_station:
		tx_bytes = ctrl_socket->send(MSG_NEW_BASE);
		break;
	case float_at_sea:
		tx_bytes = _init_float(argc, argv);
		break;
	}
	if (tx_bytes <= 0)
		exit(EXIT_FAILURE);

	timeval tv = {1, 0};
	ssize_t rx_bytes = ctrl_socket->recv(&client_id, sizeof(client_id), &tv);
	if (rx_bytes != sizeof(client_id))
		exit(EXIT_FAILURE);
	if (client_id < 0)
		exit(EXIT_FAILURE);

	char logpath[64];
	logpath[0] = '\0';
	switch (type)
	{
	case base_station:
		fprintf(stdout, "\e]0;Base #%u", client_id);
		sprintf(logpath, LOGDIR_SYMLINK "/" LOGFILE_BASE_FMT);
		break;
	case float_at_sea:
		fprintf(stdout, "\e]0;Float #%u", client_id);
		sprintf(logpath, LOGDIR_SYMLINK "/" LOGFILE_FLOAT_FMT);
		break;
	default:
		fprintf(stdout, "\e]0;???");
	}

	fprintf(stdout, ", port %u\a", ctrl_socket->port());
	fflush(stdout);

	if (logpath[0])
		log_file = fopen(logpath, "w");
	dprint("start: id %d\n", client_id);

	//init_sig_handler();

	pthread_create(&ctrl_thread, NULL, &ctrl_thread_function, NULL);
}

//-----------------------------------------------------------------------------
void init_base_client(int argc, char **argv)
{
	init_client(base_station, argc, argv);
}

void init_float_client(int argc, char **argv)
{
	init_client(float_at_sea, argc, argv);
	pthread_create(&dive_thread, NULL, &dive_thread_function, NULL);
}
