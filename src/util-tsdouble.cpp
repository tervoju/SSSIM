/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>

#include "util-tsdouble.h"

ts_double::ts_double(double _v) : v(_v), mutex() { pthread_mutex_init(&mutex, NULL); }

ts_double::~ts_double() { pthread_mutex_destroy(&mutex); }

double ts_double::get()
{
	pthread_mutex_lock(&mutex);
	double _v = v;
	pthread_mutex_unlock(&mutex);
	return _v;
}

void ts_double::set(double _v)
{
	pthread_mutex_lock(&mutex);
	v = _v;
	pthread_mutex_unlock(&mutex);
}
