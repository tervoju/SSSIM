#ifndef _ctd_estimate_h
#define _ctd_estimate_h


#define  SALT_UPDATE_RANGE   3.0
#define  SALT_ADJUST_RANGE  20.0
#define  TEMP_UPDATE_RANGE   3.0
#define  TEMP_ADJUST_RANGE  20.0

const int zvar_count = 150; //sizeof(temp_est) / sizeof(temp_est[0]);

struct zvt_struct {
	double depth;
	double value;
	uint32_t timestamp;
	zvt_struct(): depth(0.0), value(0.0), timestamp(0) { }
	zvt_struct(double _depth, double _value, unsigned int _timestamp): depth(_depth), value(_value), timestamp(_timestamp) { }
};
typedef std::vector<zvt_struct> zvt_vector;
typedef zvt_vector::iterator zvt_iter;
typedef zvt_vector::const_iterator zvt_const_iter;

void density_filter(zvt_vector & dv);
zvt_vector salt_vector(zvt_vector & dv);

struct zs_delta_s {
	double z;
	double zd;
	double sd;
	zs_delta_s(const double _z, const double _zd, const double _sd):
		z(_z),
		zd(_zd),
		sd(_sd)
	{ }
};
typedef std::vector<zs_delta_s> zs_delta_t;

struct UpdateSum {
	double value;
	double weight;

	void add(double v, double w) {
		value += v*w;
		weight += w;
	}
	double normalize() {
		if (weight <= 0.0) return value = 0.0;
		else return value /= weight;
	}
};

double estimate_temperature(double z);
void adjust_temperature_estimate(uint32_t t0, bool debug_print = false);

double estimate_salinity(double z);
double estimate_salinity(double z, double temp, double density);
void adjust_salinity_estimate(zvt_vector & density_map);

double estimate_density(double z);
double estimate_density(double z, zvt_vector & density_map, uint32_t t0);

/* Estimate sound channel depth and range
 *
 * returns TRUE for good channel, FALSE for bad/none
 * always sets z, z_above and z_below -- if return FALSE, these may be -1 if invalid
 */
bool estimate_soundchannel(int z_max, int * z, int * z_above = NULL, int * z_below = NULL);

#endif // _ctd_estimate_h
