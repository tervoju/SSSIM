/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include "util-trigger.h"

//-----------------------------------------------------------------------------
trigger_t::trigger_t(bool init_value) : mutex(), cond_true(), cond_false(), value(init_value)
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond_true, NULL);
	pthread_cond_init(&cond_false, NULL);
}

trigger_t::~trigger_t()
{
	pthread_cond_destroy(&cond_true);
	pthread_cond_destroy(&cond_false);
	pthread_mutex_destroy(&mutex);
}

//-----------------------------------------------------------------------------
void trigger_t::set(bool new_value)
{
	pthread_mutex_lock(&mutex);
	value = new_value;
	pthread_cond_broadcast(value ? &cond_true : &cond_false);
	pthread_mutex_unlock(&mutex);
}

void trigger_t::wait_for(bool it)
{
	pthread_mutex_lock(&mutex);
	if (value != it)
		pthread_cond_wait(it ? &cond_true : &cond_false, &mutex);
	pthread_mutex_unlock(&mutex);
}
