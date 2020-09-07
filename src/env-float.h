#ifndef _env_float_h
#define _env_float_h

/* requires:
	#include <stdint.h>
	#include <netcdfcpp.h>

	#include "sssim-structs.h"
	#include "util-convert.h" 
	#include "env-sea.h"
 */

#define  SIMULATOR_STEPSIZE  1.0

// used in SimFloat::Accelerate3D
#define  DRIFTER_MIN_DENSITY_NEAR_SURFACE  1000.0
#define  DRIFTER_MAX_VOLUME_NEAR_SURFACE  (DRIFTER_MASS / DRIFTER_MIN_DENSITY_NEAR_SURFACE)
#define  DRIFTER_NEAR_SURFACE_DEPTH  (2 * DRIFTER_HALF_HEIGHT)

class SimFloat {
	public:
		SimFloat(int16_t _id, const FloatState & fs);

		int16_t id;
		bool frozen;

		Vec3 pos, vel;

		double soundspeed() const { return soundspeed_from_STD(salinity, temperature, pressure_from_depth(pos[2])); }

		bool at_surface() const { return (pos[3] < 1.2); }

		void * get_info(size_t * len) const; // FloatState
		void * get_env(size_t * len) const; // T_EnvData
		void * get_env(const double depth, size_t * len) const; // T_EnvData

		void * get_ctd(size_t * len) const; // ctd_data_t
		void * get_echo(size_t * len) const; // unsigned char
		void * get_density(size_t * len) const; // size_t
		void * get_gps(size_t * len) const; // gps_data_t

		bool in_sound_channel(double * ss) const;

		bool set_density(double density);

		void Update();

	private:
		double volume, volume_goal;
		double bottom_depth, salinity, temperature, rho;
		Vec3 drift;

		void RandomizeWithMapCenter(double x0, double y0, double xr, double yr);

		void UpdateVolume(double dt);
		bool UpdateEnvironment();
		Vec3 Accelerate3D(Vec3 v, double depth);
		void StepRungeKutta4(double td);
		void StepEuler(double td);
};

std::ostream &operator << (std::ostream &s, const SimFloat &d);

#endif // _env_float_h
