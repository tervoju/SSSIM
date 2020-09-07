/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <queue>

#include "sssim.h"
#include "udp.h"
#include "float-structs.h"
#include "client-socket.h"
#include "client-time.h"
#include "client-print.h"
#include "client-init.h"
#include "util-msgqueue.h"
#include "soundmsg-fmt.h"

extern SimSocket *gui_socket;
extern SimTime sim_time;

msg_queue satmsg_rx_queue, soundmsg_rx_queue;
SimSocket *ctrl_socket = NULL;

//-----------------------------------------------------------------------------
void handle_ctrl_msg(char msg_type, const char *msg_body, ssize_t msg_len, const sockaddr_in *addr)
{
	if (msg_type == MSG_QUIT)
		exit(EXIT_SUCCESS);

	int rc = sim_time.set(msg_type, msg_body, msg_len);
	if (rc != 0)
		dprint_stdout("|| rx CTRL:%s (%zdc) from %s\n", msg_name(msg_type), msg_len, inet(*addr).str);
	switch (msg_type)
	{
	case MSG_PLAY:
		dprint_stdout("|| Time started\n");
		break;
	case MSG_PAUSE:
		dprint_stdout("|| Time paused \n");
		break;
	}
}

//-----------------------------------------------------------------------------
void handle_pong_msg(const char *buf, ssize_t len)
{
	SoundMsg msg(SOUNDMSG_PONG, buf, len);
	msg.from = SOUNDMSG__ID_MODEM;
	msg.time = sim_time.now();
	soundmsg_rx_queue.push(msg.cdata(), msg.csize());
}

//-----------------------------------------------------------------------------
void init_gui_socket(const void *buf, ssize_t len)
{
	if (len != sizeof(sockaddr_in))
	{
		dprint_error("|| rx BAD GUI address\n");
		if (log_file != NULL)
		{
			dprint_logonly("\tMSG:");
			const char *msg = reinterpret_cast<const char *>(buf);
			for (int i = 0; i < len; ++i)
				fprintf(log_file, " %02hhX", msg[i]);
			fputc('\n', log_file);
		}
		return;
	}
	const sockaddr_in *gui_addr = reinterpret_cast<const sockaddr_in *>(buf);
	//dprint_stdout("|| rx GUI address: %s\n", inet(*gui_addr).str);

	if (gui_socket != NULL)
		delete gui_socket;
	gui_socket = new SimSocket(gui_addr);
}

//-----------------------------------------------------------------------------
void *ctrl_thread_function(void *unused)
{
	//dprint_stdout(":: %s begin\n", __FUNCTION__);

	const unsigned int buf_maxlen = 1500;
	char buf[buf_maxlen];
	sockaddr_in addr;

	while (true)
	{
		ssize_t rx_bytes = ctrl_socket->recvfrom(&addr, buf, buf_maxlen);
		if (rx_bytes <= 0)
			continue;

		const char msg_type = buf[0];
		const char *msg_body = buf + 1;
		const ssize_t msg_len = rx_bytes - 1;

		//printf(VT_SET(VT_CYAN) "<< %s" VT_SET(VT_DIM) " [%zu]\n" VT_RESET, msg_name(msg_type), msg_len);

		if ((msg_type & MSG_CTRL_MASK) == MSG_CTRL_MASK)
		{
			handle_ctrl_msg(msg_type, msg_body, msg_len, &addr);
		}
		else
			switch (msg_type)
			{
			case MSG_SATMSG:
				satmsg_rx_queue.push(msg_body, msg_len);
				break;
			case MSG_SOUNDMSG:
				soundmsg_rx_queue.push(msg_body, msg_len);
				break;
			case MSG_PINGPONG:
				handle_pong_msg(msg_body, msg_len);
				break;
			case (char)(MSG_GET_GUIADDR | MSG_ACK_MASK):
				init_gui_socket(buf + 1, rx_bytes - 1);
				break;
			default:
				dprint_stdout("|| rx %s (%02hhX, %zdc) from %s\n", msg_name(msg_type), msg_type, rx_bytes, inet(addr).str);
			}
	}
	return NULL;
}
