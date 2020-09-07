/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <vector>

#include "sssim.h"
#include "satmsg-fmt.h"
#include "float-structs.h"
#include "satmsg-data.h"


//-----------------------------------------------------------------------------
template<class T> T * satmsg_init(const char * * buf, size_t * len, size_t sz_skip_from_end = 0) {
	const size_t sz = sizeof(T) - sz_skip_from_end;
	if (*len < sz) return NULL;
	T * data = new T();
	memcpy(data, *buf, sz);
	*buf += sz;
	*len -= sz;
	return data;
}

template<class T> T * satmsg_init_n(unsigned int n, const char * * buf, size_t * len) {
	const size_t sz = n * sizeof(T);
	if ((n == 0) || (*len < sz)) return NULL;
	T * data = new T[n];
	memcpy(data, *buf, sz);
	*buf += sz;
	*len -= sz;
	return data;
}


//-----------------------------------------------------------------------------
void floatmsg_t::set(unsigned char msg_type, const char * buf, size_t len) {
	if ((msg_type == 0) || (buf == NULL) || (len == 0)) return;

	if (msg_type & SATMSG_FLOAT_SELF) self = satmsg_init<self_status_t>(&buf, &len);
	if (msg_type & SATMSG_FLOAT_GROUP) {
		group = satmsg_init<rel_group_status_t>(&buf, &len, sizeof(group->floats));
		if (group && (group->n_floats > 0)) {
			group->floats = satmsg_init_n<rel_status_with_id>(group->n_floats, &buf, &len);
			if (!group->floats) group->n_floats = 0;
		}
	}
	if (msg_type & SATMSG_FLOAT_ENV) {
		env = satmsg_init<env_status_t>(&buf, &len, sizeof(env->layers));
		if (env && (env->n_layers > 0)) {
			env->layers = satmsg_init_n<env_layer_status_t>(env->n_layers, &buf, &len);
			if (!env->layers) env->n_layers = 0;
		}
	}
	//if (msg_type & SATMSG_FLOAT_RANGES)  = satmsg_init<>(&buf, &len);
}

//floatmsg_t::floatmsg_t(unsigned char msg_type, const char * buf, size_t len):
//	msg_buf(NULL), self(NULL), group(NULL), env(NULL)
//{ set(msg_type, buf, len); }

floatmsg_t::floatmsg_t(const SatMsg * sat_msg):
	msg_buf(NULL), self(NULL), group(NULL), env(NULL)
{ if (sat_msg) set(sat_msg->type, sat_msg->body, sat_msg->body_len); }


//-----------------------------------------------------------------------------
floatmsg_t::~floatmsg_t() {
	delete[] msg_buf;
	delete self;
	delete group;
	delete env;
}


//-----------------------------------------------------------------------------
unsigned char floatmsg_t::type() const {
	unsigned char c = SATMSG_FLOAT_EMPTY;
	if (self)  c |= SATMSG_FLOAT_SELF;
	if (group) c |= SATMSG_FLOAT_GROUP;
	if (env)   c |= SATMSG_FLOAT_ENV;
	return c;
}

size_t floatmsg_t::csize() const {
	size_t sz = 0;
	if (self) sz += sizeof(self_status_t);
	if (group) {
		sz += sizeof(rel_group_status_t) - sizeof(group->floats);
		sz += group->n_floats * sizeof(*group->floats);
	}
	if (env) {
		sz += sizeof(env_status_t) - sizeof(env->layers);
		sz += env->n_layers * sizeof(*env->layers);
	}
	return sz;
}

const char * floatmsg_t::cdata() {
	delete[] msg_buf;
	msg_buf = new char[csize()];

	void * b = msg_buf;

	if (self) b = mempcpy(b, self, sizeof(*self));
	if (group) {
		b = mempcpy(b, group, sizeof(*group) - sizeof(group->floats));
		if (group->n_floats) b = mempcpy(b, group->floats, group->n_floats * sizeof(*group->floats));
	}
	if (env) {
		b = mempcpy(b, env, sizeof(*env) - sizeof(env->layers));
		if (env->n_layers) b = mempcpy(b, env->layers, env->n_layers * sizeof(*env->layers));
	}

	return msg_buf;
}


//-----------------------------------------------------------------------------
void basemsg_t::set(unsigned char msg_type, const char * buf, size_t len) {
	if ((msg_type == 0) || (buf == NULL) || (len == 0)) return;

	if (msg_type & SATMSG_BASE_GO_INIT)  set_init = satmsg_init<init_params_t>(&buf, &len);
	if (msg_type & SATMSG_BASE_GO_CYCLE) set_cycle = satmsg_init<cycle_params_t>(&buf, &len);
	if (msg_type & SATMSG_BASE_GROUP) {
		group.clear();
		const void * _buf = buf;
		while (len >= abs_pos_with_id::char_len()) {
			abs_pos_with_id a;
			size_t n = 0;
			_buf = a.read(_buf, true, &n);
			len -= n;
			group.push_back(a);
		}
	}
}

basemsg_t::basemsg_t(const SatMsg * sat_msg):
	msg_buf(NULL), set_init(NULL), set_cycle(NULL), group()
{ if (sat_msg) set(sat_msg->type, sat_msg->body, sat_msg->body_len); }


//-----------------------------------------------------------------------------
basemsg_t::~basemsg_t() {
	delete[] msg_buf;
	delete set_init;
	delete set_cycle;
}


//-----------------------------------------------------------------------------
unsigned char basemsg_t::type() const {
	unsigned char c = SATMSG_BASE_EMPTY;
	if (set_init)       c |= SATMSG_BASE_GO_INIT;
	if (set_cycle)      c |= SATMSG_BASE_GO_CYCLE;
	if (!group.empty()) c |= SATMSG_BASE_GROUP;
	return c;
}

size_t basemsg_t::csize() const {
	size_t sz = 0;
	if (set_init)       sz += sizeof(*set_init);
	if (set_cycle)      sz += sizeof(*set_cycle);
	if (!group.empty()) sz += group.size() * abs_pos_with_id::char_len();
	return sz;
}

const char * basemsg_t::cdata() {
	delete[] msg_buf;
	msg_buf = new char[csize()];

	void * b = msg_buf;

	if (set_init)       b = mempcpy(b, set_init,  sizeof(*set_init));
	if (set_cycle)      b = mempcpy(b, set_cycle, sizeof(*set_cycle));
	if (!group.empty()) { FOREACH_CONST(it, group) b = it->write(b); }

	return msg_buf;
}


//-----------------------------------------------------------------------------
char * satmsg_type2str(unsigned char msg_type, char * s0, ssize_t max_len) {
	int rc;

	switch (msg_type) {
		case SATMSG__OK:              rc = snprintf(s0, max_len, "ACK ok");          return (rc < 0) ? NULL : s0;
		case SATMSG__ERROR_NO_SIGNAL: rc = snprintf(s0, max_len, "ERROR no signal"); return (rc < 0) ? NULL : s0;
		case SATMSG__ERROR_BAD_ID:    rc = snprintf(s0, max_len, "ERROR bad id");    return (rc < 0) ? NULL : s0;
		case SATMSG__ERROR_BAD_DATA:  rc = snprintf(s0, max_len, "ERROR bas data");  return (rc < 0) ? NULL : s0;
	}

	char * s = s0;
	for (int i = 0x01; i <= 0x80; i <<= 1) if ((i == SATMSG_SOURCE) || (msg_type & i)) {
		if (msg_type & SATMSG_SOURCE) switch (i) {
			case SATMSG_SOURCE:         rc = snprintf(s, max_len, "BASE"); break;
			case SATMSG_BASE_GO_INIT:   rc = snprintf(s, max_len, " go_init"); break;
			case SATMSG_BASE_GO_CYCLE:  rc = snprintf(s, max_len, " go_cycle"); break;
			case SATMSG_BASE_GO_END:    rc = snprintf(s, max_len, " go_end"); break;
			case SATMSG_BASE_SELF:      rc = snprintf(s, max_len, " self"); break;
			case SATMSG_BASE_GROUP:     rc = snprintf(s, max_len, " group"); break;
			case SATMSG_BASE_ENV:       rc = snprintf(s, max_len, " env"); break;
			default:                    rc = snprintf(s, max_len, " UNKNOWN(%02hhX)", i); break;
		} else switch(i) {
			case SATMSG_SOURCE:         rc = snprintf(s, max_len, "FLOAT"); break;
			case SATMSG_FLOAT_SELF:     rc = snprintf(s, max_len, " self"); break;
			case SATMSG_FLOAT_GROUP:    rc = snprintf(s, max_len, " group"); break;
			case SATMSG_FLOAT_ENV:      rc = snprintf(s, max_len, " env"); break;
			case SATMSG_FLOAT_RANGES:   rc = snprintf(s, max_len, " ranges"); break;
			default:                    rc = snprintf(s, max_len, " UNKNOWN(%02hhX)", i); break;
		}
		if (rc < 0) return NULL;
		else if (rc >= max_len) return s0;
		max_len -= rc;
		s += rc;
	}

	return s0;
}
