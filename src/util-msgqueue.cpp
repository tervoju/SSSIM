/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <stdint.h>
#include <queue>
#include <pthread.h>

#ifndef _sssim_h
	typedef uint32_t simtime_t;
#endif

#include "util-msgqueue.h"


// NOTE/FIXME: queues are sorted by order of insertion, so out-of-order
//             reception behaviour isn't.

// FIXME: should use lock-free code:
//			http://www.drdobbs.com/high-performance-computing/210604448	
//			http://www.ddj.com/cpp/211601363

//-----------------------------------------------------------------------------
msg_queue::msg_queue():
	max_length(-1),
	queue(),
	mutex(),
	push_cond()
{ 
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&push_cond, NULL);
}

msg_queue::~msg_queue() { 
	pthread_cond_destroy(&push_cond);
	pthread_mutex_destroy(&mutex); 
}


//-----------------------------------------------------------------------------
void msg_queue::set_max_length(ssize_t _max_length) {
	max_length = _max_length; 
}

bool msg_queue::empty() const { 
	return queue.empty();
}

void msg_queue::clear() {
	std::queue<t_msg> _empty;
	pthread_mutex_lock(&mutex);
		std::swap(queue, _empty);
	pthread_mutex_unlock(&mutex);
}


//-----------------------------------------------------------------------------
bool msg_queue::push(const char * buf, ssize_t len) {
	if (len < 0) return false;
	if ((max_length >= 0) && (queue.size() > static_cast<size_t>(max_length))) return false;
	pthread_mutex_lock(&mutex);
		queue.push(t_msg(buf, buf + len));
		pthread_cond_broadcast(&push_cond);
	pthread_mutex_unlock(&mutex);
	return true;
}


//-----------------------------------------------------------------------------
int msg_queue::wait_for_msg(long timeout_ns) {
	if (!queue.empty()) return 0;

	int rc = 0;
	pthread_mutex_lock(&mutex);
		if (timeout_ns > 0) {
			const timespec ts = { timeout_ns / 1e9l, timeout_ns };
			rc = pthread_cond_timedwait(&push_cond, &mutex, &ts);
		} else {
			rc = pthread_cond_wait(&push_cond, &mutex);
		}
	pthread_mutex_unlock(&mutex);

	return rc;
}


//-----------------------------------------------------------------------------
t_msg msg_queue::_get(bool pop) {
	if (queue.empty()) return t_msg();
	pthread_mutex_lock(&mutex);
		t_msg msg(queue.front());
		if (pop) queue.pop();
	pthread_mutex_unlock(&mutex);
	return msg;
}
