/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <vector>
#include <libconfig.h++>

#include "sssim.h"
#include "satmsg-fmt.h"
#include "float-structs.h"
#include "satmsg-data.h"

#define CFG_FILE "cfg/base.cfg"

bool read_cfg(basemsg_t *msg, bool set_init, bool set_cycle)
{
	try
	{
		libconfig::Config cfg;
		cfg.readFile(CFG_FILE);

		if (set_init)
		{
			if (!msg->set_init)
				msg->set_init = new init_params_t();
			msg->set_init->d_start = cfg.lookup("init.density.start");
			msg->set_init->d_step = cfg.lookup("init.density.step");
			msg->set_init->d_end = cfg.lookup("init.density.end");
			msg->set_init->z_max = cfg.lookup("init.max_depth");
			msg->set_init->t_length = cfg.lookup("init.time.max");
			msg->set_init->t_buffer = cfg.lookup("init.time.rise_buffer");
		}

		if (set_cycle)
		{
			if (!msg->set_cycle)
				msg->set_cycle = new cycle_params_t();
			msg->set_cycle->t_period = cfg.lookup("cycle.period");
			msg->set_cycle->tx_slot_seconds = static_cast<unsigned int>(cfg.lookup("cycle.tx_slot_seconds"));
		}

		return true;
	}
	catch (libconfig::SettingTypeException &x)
	{
		printf("config error: bad setting type for %s\n", x.getPath());
	}
	catch (libconfig::SettingNotFoundException &x)
	{
		printf("config error: setting not found: %s\n", x.getPath());
	}
	catch (libconfig::SettingNameException &x)
	{
		printf("config error: invalid setting name \"%s\"\n", x.getPath());
	}
	catch (libconfig::ParseException &x)
	{
		printf("config error: %s at line %d\n", x.getError(), x.getLine());
	}
	catch (libconfig::FileIOException &x)
	{
		printf("config error: file I/O error\n");
	}
	catch (libconfig::ConfigException &x)
	{
		printf("config error: unknown error\n");
	}

	return false;
}
bool read_init_cfg(basemsg_t *msg) { return read_cfg(msg, true, false); }
bool read_cycle_cfg(basemsg_t *msg) { return read_cfg(msg, false, true); }
