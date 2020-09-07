/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <stdint.h>

#include "satmsg-fmt.h"


//-----------------------------------------------------------------------------
SatMsg::SatMsg():
	ca(NULL),
	from(-1), to(-1),
	tx_time(0),
	msg_id(0), ack_id(0),
	body_len(0),
	type(0),
	body(NULL)
{ }


//-----------------------------------------------------------------------------
SatMsg::SatMsg(unsigned char _type, const void * _body, size_t _body_len, int16_t _to):
	ca(NULL),
	from(-1), to(_to),
	tx_time(0),
	msg_id(0), ack_id(0),
	body_len(0),
	type(_type),
	body(NULL)
{
	if ((_body != NULL) && (_body_len > 0)) {
		body_len = _body_len;
		body = new char[body_len];
		memcpy(body, _body, body_len);
	}
}


//-----------------------------------------------------------------------------
SatMsg::SatMsg(const char * data, size_t len):
	ca(NULL),
	from(-1), to(-1),
	tx_time(0),
	msg_id(0), ack_id(0),
	body_len(0),
	type(0),
	body(NULL)
{
	if (len < csize()) throw data_err();

	const char * ci = data;
	memcpy(&from, ci, sizeof(from));       ci += sizeof(from);
	memcpy(&to, ci, sizeof(to));           ci += sizeof(to);
	memcpy(&tx_time, ci, sizeof(tx_time)); ci += sizeof(tx_time);
	memcpy(&msg_id, ci, sizeof(msg_id));   ci += sizeof(msg_id);
	memcpy(&ack_id, ci, sizeof(ack_id));   ci += sizeof(ack_id);
	memcpy(&type, ci, sizeof(type));       ci += sizeof(type);

	body_len = len - csize();
	if (body_len > 0) {
		body = new char[body_len];
		memcpy(body, ci, body_len);
	}
}


//-----------------------------------------------------------------------------
SatMsg::SatMsg(const SatMsg & _):
	ca(NULL),
	from(_.from), to(_.to),
	tx_time(_.tx_time),
	msg_id(_.msg_id), ack_id(_.ack_id),
	body_len(0),
	type(_.type),
	body(NULL)
{
	if ((_.body != NULL) && (_.body_len > 0)) {
		body_len = _.body_len;
		body = new char[body_len];
		memcpy(body, _.body, body_len);
	}
}


//-----------------------------------------------------------------------------
SatMsg & SatMsg::operator= (const SatMsg & _) {
	if (this != &_) {
		delete[] ca; 
		ca = NULL;

		from = _.from;
		to = _.to;
		tx_time = _.tx_time;
		msg_id = _.msg_id;
		ack_id = _.ack_id;
		type = _.type;

		delete[] body;
		if ((_.body != NULL) && (_.body_len > 0)) {
			body_len = _.body_len;
			body = new char[body_len];
			memcpy(body, _.body, body_len);
		} else {
			body_len = 0;
			body = NULL;
		}
	}

	return *this;
}


//-----------------------------------------------------------------------------
SatMsg::~SatMsg() { 
	delete[] ca; 
	delete[] body;
}


//-----------------------------------------------------------------------------
size_t SatMsg::csize() const {
	return sizeof(from) + sizeof(to) + sizeof(tx_time) + sizeof(msg_id) + sizeof(ack_id) + sizeof(type) + body_len;
}

char * SatMsg::cdata(size_t prefix) {
	if (ca != NULL) delete[] ca;
	ca = new char[prefix + csize()];
	if (prefix > 0) memset(ca, 0, prefix);

	void * ci = ca + prefix;
	ci = mempcpy(ci, &from, sizeof(from));
	ci = mempcpy(ci, &to, sizeof(to));
	ci = mempcpy(ci, &tx_time, sizeof(tx_time));
	ci = mempcpy(ci, &msg_id, sizeof(msg_id));
	ci = mempcpy(ci, &ack_id, sizeof(ack_id));
	ci = mempcpy(ci, &type, sizeof(type));
	if (body_len > 0) memcpy(ci, body, body_len);

	return ca;
}
