/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <stdint.h>

#include "soundmsg-fmt.h"


//-----------------------------------------------------------------------------
SoundMsg::SoundMsg(char _type, const void * _body, char _body_len, char _id):
	ca(NULL),
	from(-1),
	time(0),
	type(_type),
	id(_id),
	body_len(0),
	body(NULL)
{
	if ((_body != NULL) && (_body_len > 0)) {
		body_len = _body_len;
		body = new char[body_len];
		memcpy(body, _body, body_len);
	}
}

SoundMsg::SoundMsg(const char * data, size_t len):
	ca(NULL),
	from(-1),
	time(0),
	type(0),
	id(-1),
	body_len(0),
	body(NULL)
{
	if (len < csize()) throw data_err(); //return;

	const char * ci = data;
	memcpy(&from, ci, sizeof(from)); ci += sizeof(from);
	memcpy(&time, ci, sizeof(time)); ci += sizeof(time);
	memcpy(&type, ci, sizeof(type)); ci += sizeof(type);
	memcpy(&id,   ci, sizeof(id));   ci += sizeof(id);

	body_len = len - csize();
	if (body_len > 0) {
		body = new char[body_len];
		memcpy(body, ci, body_len);
	}
}


//-----------------------------------------------------------------------------
SoundMsg::SoundMsg(const SoundMsg & _):
	ca(NULL),
	from(_.from),
	time(_.time),
	type(_.type),
	id(_.id),
	body_len(0),
	body(NULL)
{
	if ((_.body != NULL) && (_.body_len > 0)) {
		body_len = _.body_len;
		body = new char[body_len];
		memcpy(body, _.body, body_len);
	}
}

SoundMsg & SoundMsg::operator= (const SoundMsg & _) {
	from = _.from;
	time = _.time;
	type = _.type;
	id = _.id;

	delete[] body;
	if ((_.body != NULL) && (_.body_len > 0)) {
		body_len = _.body_len;
		body = new char[body_len];
		memcpy(body, _.body, body_len);
	} else {
		body_len = 0;
		body = NULL;
	}

	return *this;
}


//-----------------------------------------------------------------------------
SoundMsg::~SoundMsg() { 
	delete[] ca; 
	delete[] body;
}


//-----------------------------------------------------------------------------
size_t SoundMsg::csize() const {
	return sizeof(from) + sizeof(time) + sizeof(type) + sizeof(id) + body_len;
}

char * SoundMsg::cdata(size_t prefix) {
	delete[] ca;
	ca = new char[prefix + csize()];
	if (prefix > 0) memset(ca, 0, prefix);

	void * ci = ca + prefix;
	ci = mempcpy(ci, &from, sizeof(from));
	ci = mempcpy(ci, &time, sizeof(time));
	ci = mempcpy(ci, &type, sizeof(type));
	ci = mempcpy(ci, &id, sizeof(id));
	if (body_len > 0) memcpy(ci, body, body_len);

	return ca;
}
