#ifndef _env_sea_h
#define _env_sea_h


struct sea_locdim_t {
	unsigned int cell;
	double frac;
};

struct sea_lim_t { double       t, z, y, x; };
struct sea_loc_t { sea_locdim_t t, z, y, x; };

typedef double bottom_data_t[SEA_DEPTH_NY][SEA_DEPTH_NX];
typedef double sea_data_t[SEA_NT][SEA_NZ][SEA_NY][SEA_NX];


class Sea {
	enum Vec3_dims_t { X, Y, Z };

	public:
		sea_data_t salt_data, temp_data, flow_data[3];
		sea_lim_t min, step, max_loc;

		bottom_data_t bottom_data;
		sea_lim_t bottom_min, bottom_step;

		double depth_data[SEA_NZ];	// 0: deepest, SEA_NZ-1: shallowest

		Sea();

		double bottom(const double * pos) const;
		bool variables(const double * pos, const double t, double * salinity, double * temperature, double * drift) const;
		bool driftvals(const double * pos, const double t, double *drift) const;

		double soundspeed(const double * pos, const double t) const;
		double min_soundspeed(const double * pos, const double t, double max_depth, double * min_soundspeed_depth = NULL) const;

	private:
		bool ReadVar(const char * var_name, const char * fn, sea_data_t & data);
		void ReadDimensions(const char * fn);
		void AdjustRanges();
		bool ReadDepth(const char * fn);

		bool map(const double * pos, const double t, sea_loc_t & loc) const;
		double value(const sea_loc_t loc, const sea_data_t & data) const;
		double soundspeed(const sea_loc_t loc, double z) const;
};

#endif
