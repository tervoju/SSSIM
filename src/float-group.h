#ifndef _float_group_h
#define _float_group_h


enum r_conf_enum { RANGE_ESTIMATED, RANGE_MEASURED, RANGE_GPS };
enum b_conf_enum { BEARING_UNKNOWN, BEARING_UNCERTAIN, BEARING_CERTAIN };

class groupfloat {
public:
	static const int max_cycle_count = 512; // ok for 40 days @ 2h/cycle
	depthcode depthcodes[max_cycle_count];

	pos_xyt_conf gps_pos;
	pos_xyzt gps_est;

	groupfloat();

	depthcode get_depthcode(int c);
	void set_depthcode(int c, depthcode d);

	void add_gps_measurement(simtime_t time, double x, double y, Seafloat * seafloat);
	void add_gps_estimate(simtime_t time, double x, double y, double z, Seafloat * seafloat);
};

#endif
