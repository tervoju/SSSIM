#ifndef _sssim_structs_h
#define _sssim_structs_h

/* requires:
	#include <stdint.h>
	#include <svl/SVL.h>
 */


struct FloatState {
	Vec3 pos, vel;
	double volume;
	double bottom_depth;
	int id;
	int padding; // to match sizeof() between 32-bit & 64-bit systems

	FloatState():
		pos ( vl_zero ),
		vel ( vl_zero ),
		//err_pos ( vl_zero ),
		//pred_pos ( vl_zero ),
		volume ( 0.0 ),
		bottom_depth( 0.0 ),
		id ( 0 ),
		padding( 0 )
	{ }
};
std::ostream & operator << (std::ostream &s, const FloatState &ds);

#ifndef _ctd_tracker_h
	struct ctd_data_t {
		double conductivity;	// mS/cm
		double temperature;		// ITS-90
		unsigned int pressure;	// millibar
		uint32_t time;		// seconds
	};
#endif // _ctd_tracker_h
std::ostream & operator << (std::ostream &s, const ctd_data_t &ctd);

#endif // _sssim_structs_h
