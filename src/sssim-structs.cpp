/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <svl/SVL.h>

#include "sssim-structs.h"


std::ostream & operator << (std::ostream &s, const FloatState &ds) {
	int p = s.precision();
	s.setf(s.fixed, s.floatfield);
	//s << '#' << ds.id << ": ";
	s.precision(4);
	if (ds.volume > 0.0) s << "volume " << ds.volume << ", ";
	s.precision(1);
	s << "pos " << ds.pos;
	s.unsetf(s.floatfield);
	s.precision(p);
	if (ds.vel != vl_zero) s << ", vel " << ds.vel;
    return s;
}

std::ostream & operator << (std::ostream &s, const ctd_data_t &ctd) {
	return s << ctd.time << ": "<< ctd.temperature << "°C, " << ctd.pressure << " mbar, " << ctd.conductivity << " mS/cm";
}
