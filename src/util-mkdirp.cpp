/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <stdlib.h>
#include <fcntl.h>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int mkdirp(const char * path, mode_t mode) {
	char _path[256];
	if (strlen(path) >= sizeof(_path)) { errno = ENAMETOOLONG; return -1; }
	strncpy(_path, path, sizeof(_path));
	if (_path[strlen(_path) - 1] == '/') _path[strlen(_path) - 1] = '\0';

	for (char * p = _path + 1; *p; ++p) if (*p == '/') {
		*p = '\0';
		if (mkdir(_path,  mode) < 0) return -1;
		*p = '/';
	}
	return mkdir(_path,  mode);
}
