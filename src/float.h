#ifndef _float_h
#define _float_h

#include <list>
#include "kalman_filter.h"
// ADD JTE
// struct distance list
struct distance_float {
	double d;
	simtime_t time;
	int floatId;
	distance_float(double _d=0.0, simtime_t _time=0, int _floatId=0):d(_d), time(_time), floatId(_floatId) {}
};

// struct for positions list
struct pos_xyzt {
	double x, y, z;
    simtime_t time;
	int floatId; 
	pos_xyzt(double _x = 0.0, double _y = 0.0, double _z = 0.0, simtime_t _time = 0, int _floatId = 0): x(_x), y(_y), z(_z), time(_time), floatId(_floatId) { }

};
struct pos_xyt_conf {
	double x, y;
	simtime_t time;
	double conf;
	pos_xyt_conf(double _x = 0.0, double _y = 0.0, simtime_t _time = 0, double _conf = 0.0): x(_x), y(_y), time(_time), conf(_conf) { }
};

class groupfloat;
typedef std::map<int16_t, groupfloat> group_t;

// class
//-----------------------------------------------------------------------------
class Seafloat {
public:
	// operation
	bool env_init_done;
	init_params_t init;

	// group
	pos_xyt_conf abs_pos;
	group_t group;
	groupfloat * self_in_group;

	// comms
	std::vector<double> env_depths;
	simtime_t latest_sat_contact;
	SatModem sat_modem;
	SoundModem snd_modem;

	// etc
	GPSclient gps;
	void setEstimatedPosition(simtime_t, double, double, double);
	
	std::list<distance_float> floatDistances;	
	std::list<pos_xyzt> floatPositions; // list for fixes
	
	// ADD JTE
	// stream info: array of [3][1024] vect_3 e.g. 10 km area around last fix
	Seafloat(msg_queue * _satmsg_rx_queue, msg_queue * _soundmsg_rx_queue);

	double soundspeed_in_sc() const;
	bool tx_msg_to_base();
	bool rx_msg_from_base(simtime_t t_end);
	unsigned char read_basemsg(const SatMsg * sat_msg);

	int read_snd_msgs();
	void read_snd_msg(const SoundMsg * msg);

	// kalman filter handling
	void initkalman(); // 
	void updateKalmanPos(int float_nro, double, double, double);
	void updateKalmanDist(int this_unit, int float_i, double distance);
	void updateKalmanWithData();
	void updateDistances();
	void updatePositions();
	pos_xyzt getKalmanEstimation(int);
    void runKalman();
	void readKalman();
	void killKalman();
	bool initkalmandone;	

	void update_gui_predictions();
};

// states
//-----------------------------------------------------------------------------
enum state_t {
	STATE_FAIL,
	STATE_BASE_TALK,
	STATE_INIT_DIVE,
	STATE_GROUP_TALK,
	STATE_DRIFT,
	STATE_PROFILE,
	NUM_STATES 
};

typedef state_t state_func_t(Seafloat * seafloat);

state_t do_fail(Seafloat * seafloat);
state_t do_base_talk(Seafloat * seafloat);
state_t do_init_dive(Seafloat * seafloat);
state_t do_group_talk(Seafloat * seafloat);
state_t do_drift(Seafloat * seafloat);
state_t do_profile(Seafloat * seafloat);

state_func_t * const state_table[NUM_STATES] = { 
	do_fail,
	do_base_talk,
	do_init_dive,
	do_group_talk,
	do_drift,
	do_profile
};

#endif
