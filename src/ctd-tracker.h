#ifndef _ctd_tracker_h
#define _ctd_tracker_h

#define  DRIFTER_HALF_HEIGHT  1.0
#define  DRIFTER_SURFACE_DEPTH  1.5
#define  DRIFTER_SURFACE_TOLERANCE_MIN_READINGS  3 // n-1 readings at < max depth taken as sensor error
#define  DRIFTER_SURFACE_TOLERANCE_MAX_DEPTH  2.5


#ifndef _sssim_structs_h
	struct ctd_data_t {
		double conductivity;	// mS/cm
		double temperature;		// ITS-90
		unsigned int pressure;	// millibar
		uint32_t time;		// seconds
	};
#endif


struct DepthState {
	double depth_now;   // positive down, meters
	double slope;       // positive down, m/s
	double variance;
	double time_valid;  // backwards from present, seconds
};


#define  CTD_PRED_COUNT  3
typedef std::deque<ctd_data_t> ctd_history_t;
typedef ctd_history_t::iterator ctd_iter_t;

class CTDTracker {
	private:
		unsigned int max_size;
		unsigned int max_variance;

		unsigned int pred_length[CTD_PRED_COUNT];
		unsigned int pred_min_length[CTD_PRED_COUNT];
		unsigned int pred_max_length; // less than max_size

		DepthState depth_state;
		pthread_mutex_t mutex;

		// at surface
		uint32_t surfacing_time;
		double max_surf_p();
		bool surface_ds_ok();

		// underwater
		LeastSquaresTracker lst;
		bool validate_range(uint32_t test_length, uint32_t test_end);
		bool validate_state(int * first_valid);
		bool underwater_ds_ok();

	public:
		CTDTracker(); ~CTDTracker();

		double atm_p;
		double depth(double p) { return depth_from_pressure(p - atm_p); }

		ctd_history_t data;
		int lock_data() { return pthread_mutex_lock(&mutex); }
		int unlock_data() { return pthread_mutex_unlock(&mutex); }

		// the following are thread-safe
		bool add(const ctd_data_t & ctd_new);
		void reset(bool clear_data = false);
		ctd_data_t get_latest_ctd();
		DepthState get_depth_state();
		double depth_now();
};

extern CTDTracker ctd;

#endif
