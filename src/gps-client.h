#ifndef _gps_client_h
#define _gps_client_h


/*
struct gps_data_t {
	double x;
	double y;
	uint32_t t;
	int id;
	bool ok;
};
*/

class GPSclient: public SatClient {
	private:
		gps_data_t data;

	public:
		GPSclient(simtime_t _refresh_time = 60, simtime_t _connection_mean_time = 60);

		void update();
		bool get_position(gps_data_t * gps_pos, bool ignore_state = false);
};

#endif
