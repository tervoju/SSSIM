/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring> // for NULL pointer
#include <stdint.h> // for (u?)int\d+_t

#include "sssim.h"
#include "float-structs.h"


//-----------------------------------------------------------------------------
const void * abs_pos_t::read(const void * src, bool include_t, size_t * len) {
	const size_t n = include_t ? 8 : 5;

	memcpy(this, src, n);
	if (!include_t) t = 0xFFFFFF;

	if (len) *len = n;
	return reinterpret_cast<const char *>(src) + n;
}

void * abs_pos_t::write(void * tgt, bool include_t, size_t * len) const {
	const size_t n = include_t ? 8 : 5;

	if (len) *len = n;
	return mempcpy(tgt, this, n);
}


//-----------------------------------------------------------------------------
const void * abs_pos_with_id::read(const void * src, bool include_t, size_t * len) {
	const int16_t * s = reinterpret_cast<const int16_t *>(src);

	size_t n;
	id = *s++;
	abs_pos_t::read(s, include_t, &n);
	n += sizeof(*s);

	if (len) *len = n;
	return reinterpret_cast<const char *>(src) + n;
}

void * abs_pos_with_id::write(void * tgt, bool include_t, size_t * len) const {
	tgt = mempcpy(tgt, &id, sizeof(id));
	if (len) *len = sizeof(id) + (include_t ? 8 : 5);
	return abs_pos_t::write(tgt, include_t);
}


//-----------------------------------------------------------------------------
// stream format:
//   [11ff ffff]  ccmr rrrr  rrrr rrrr  [bbbb bbbb]
//   flags & bearing are optional, determined by LSB of first bytes

const void * rel_status_t::read(const void * src, size_t * len) {
	const unsigned char * s = reinterpret_cast<const unsigned char *>(src);

	size_t n = 0;
	if (reinterpret_cast<const rel_status_t *>(src)->flag_header == 0x3) {
		// 1st byte contains flags if first bits are 11
		memcpy(this, src, 3);
		n = 3;
	} else {
		// no flag header
		flag_header = 0x3;
		flags = 0x00;
		memcpy(char_ptr() + 1, src, 2);
		n = 2;
	}

	if (bearing_conf > 0) {
		// 3rd byte after possible flags has bearing, if bearing_conf is 1 or 2
		memcpy(char_ptr() + 3, s + n, 1);
		n += 1;
	}

	if (len) *len = n;
	return s + n;
}

void * rel_status_t::write(void * tgt, size_t * len) {
	if (len) *len = char_len();
	return mempcpy(tgt, char_ptr() + (flags ? 0 : 1), char_len());
}


//-----------------------------------------------------------------------------
// stream format:
//   iiii iiii  iiii iiii  [11ff ffff]  ccmr rrrr  rrrr rrrr  [bbbb bbbb]
//   flags & bearing are optional, determined by LSB of first bytes

const void * rel_status_with_id::read(const void * src, size_t * len) {
	const int16_t * s = reinterpret_cast<const int16_t *>(src);

	size_t n;
	id = *s++;
	rel_status_t::read(s, &n);
	n += sizeof(*s);

	if (len) *len = n;
	return reinterpret_cast<const char *>(src) + n;
}

void * rel_status_with_id::write(void * tgt, size_t * len) {
	tgt = mempcpy(tgt, &id, sizeof(id));
	if (len) *len = sizeof(id) + rel_status_t::char_len();
	return mempcpy(tgt, rel_status_t::char_ptr() + (flags ? 0 : 1), rel_status_t::char_len());
}


//-----------------------------------------------------------------------------
/*
#include <stdio.h>
int main() {
	{
		printf("\nABS_POS_T: sizeof = %zu\n", sizeof(abs_pos_t));

		abs_pos_t a(1, 0, 0);
		printf("x  : %li\n", a.x);
		printf("y  : %li\n", a.y);
		printf("t  : %lu\n", a.t);
		printf("--------------------\n");

		char * s = new char[8];
		size_t l_w = 0;
		memset(s, 0x00, sizeof(s));
		printf("stream :"); for (size_t i = 0; i < sizeof(s); ++i) printf(" %02hhX", s[i]); putchar('\n');

		a.write(s, true, &l_w);
		printf("write : %zu bytes\n", l_w);
		printf("stream :"); for (size_t i = 0; i < sizeof(s); ++i) printf(" %02hhX", s[i]); putchar('\n');

		abs_pos_t b;
		size_t l_r = 0;
		b.read(s, true, &l_r);
		printf("read  : %zu bytes\n", l_r);
		printf("stream :"); for (size_t i = 0; i < sizeof(s); ++i) printf(" %02hhX", s[i]); putchar('\n');

		printf("--------------------\n");
		printf("x  : %li\n", b.x);
		printf("y  : %li\n", b.y);
		printf("t  : %lu\n", b.t);
	}

	{
		printf("\nABS_POS_WITH_ID\n");

		abs_pos_with_id a(1, 2, 3, 4);
		printf("id : %i\n", a.id);
		printf("x  : %li\n", a.x);
		printf("y  : %li\n", a.y);
		printf("t  : %lu\n", a.t);
		printf("--------------------\n");

		char * s = new char[abs_pos_with_id::char_len()];
		size_t l_w = 0;
		memset(s, 0x00, sizeof(s));
		a.write(s, &l_w);

		abs_pos_with_id b;
		size_t l_r = 0;
		b.read(s, &l_r);

		printf("write : %zu bytes\n", l_w);
		printf("read  : %zu bytes\n", l_r);

		printf("stream :"); for (size_t i = 0; i < sizeof(s); ++i) printf(" %02hhX", s[i]); putchar('\n');

		printf("--------------------\n");
		printf("id : %i\n", b.id);
		printf("x  : %li\n", b.x);
		printf("y  : %li\n", b.y);
		printf("t  : %lu\n", b.t);
	}
}
*/
