/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <stdint.h>
#include <stdlib.h>

#include "sssim.h"

char *time2str(uint32_t time)
{
	char *str;
	int c = asprintf(&str, "%02d|%02d:%02d:%02d",
					 time / (3600 * 24),
					 (time % (3600 * 24)) / 3600,
					 (time % 3600) / 60,
					 (time % 60));
	return (c > 0) ? str : NULL;
}

bool is_printable_ascii(const char c) { return (c < 127) && (c > 6) && ((c < 14) || (c > 31)); }

const char *msg_name(const unsigned char c)
{
	if (c & MSG_ACK_MASK)
		switch (c & ~MSG_ACK_MASK)
		{
		case 0:
			return "ACK";
		case MSG_GET_GUIADDR:
			return "ACK:GET_GUIADDR";
		default:
			return "ACK:[unrecognised]";
		}
	switch (c)
	{
	case MSG_QUIT:
		return "QUIT";
	case MSG_PLAY:
		return "PLAY";
	case MSG_PAUSE:
		return "PAUSE";
	case MSG_ERROR:
		return "ERROR";
	case MSG_NEW_FLOAT:
		return "NEW_FLOAT";
	case MSG_NEW_BASE:
		return "NEW_BASE";
	case MSG_NEW_LOG:
		return "NEW_LOG";
	case MSG_TIME:
		return "TIME";
	case MSG_SLEEP:
		return "SLEEP";
	case MSG_SET_DENSITY:
		return "SET_DENSITY";
	case MSG_SATMSG:
		return "SAT_MSG";
	case MSG_SOUNDMSG:
		return "SOUND_MSG";
	case MSG_PINGPONG:
		return "PINGPONG";
	case MSG_SET_ID:
		return "SET_ID";
	case MSG_ENV_STATE:
		return "ENV_STATE";
	case MSG_FLOAT_STATE:
		return "FLOAT_STATE";
	case MSG_GET_GUIADDR:
		return "GET_GUIADDR";
	case MSG_GET_CTD:
		return "GET_CTD";
	case MSG_GET_ECHO:
		return "GET_ECHO";
	case MSG_GET_DENSITY:
		return "GET_DENSITY";
	case MSG_GET_GPS:
		return "GET_GPS";
	case MSG_GET_INFO:
		return "GET_INFO";
	case MSG_GET_ENV:
		return "GET_ENV";
	case MSG_GET_PROFILE:
		return "GET_PROFILE";
	case MSG_GET_DDEPTH:
		return "GET_DDEPTH";
	default:
		return "[unrecognised]";
	}
}

void print(gps_data_t *gps)
{
	char *time_str = time2str(gps->t);
	printf("%7.4f°E %7.4f°N at %s%s",
		   meters_east_to_degrees(gps->x), meters_north_to_degrees(gps->y),
		   time_str, gps->ok ? "" : " -- NOT OK");
	free(time_str);
}
