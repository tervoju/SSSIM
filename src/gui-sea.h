#ifndef _gui_sea_h
#define _gui_sea_h


#define  SEA_CLIP_MIN  30
#define  SEA_CLIP_MAX  300000


typedef double SeaBottomData[SEA_DEPTH_NY][SEA_DEPTH_NX];
typedef double SeaLayerData[SEA_NY][SEA_NX];
typedef SeaLayerData SeaData[SEA_NZ];

class GuiSea {
	public:
		double t_now;

		SeaBottomData bottom;
		SeaData salt, temp, flow[3];

		double depth[SEA_NZ];	// 0: deepest, SEA_NZ-1: shallowest
		double bottom_pos[2][2], var_pos[2][2];	// 0,0: x-offset, 0,1:x-scale 1,0: y-offset, 1,1:y-scale

		GuiSea();
		void SetTime( double t, bool update_grid );
		void PrintInfo();

	private:
		NcFile *pfDepth, *pfS, *pfTemp, *pfU, *pfV, *pfW;
		NcVar *dDepth, *dS, *dTemp, *dU, *dV, *dW;

		SeaData raw_salt[2], raw_temp[2], raw_flow[3][2];

		uint32_t t0, time_stepsize;
		uint32_t t_now_int;
		double t_now_frac;

		void ReadDepthInfo();
		void VerifyNetCDF( NcFile * pf, unsigned int level, std::string s );
		void UpdateDataToTime( SeaData & tgt, SeaData * raw );

		bool MapPosition(const double pos[], unsigned int map_cell[], double cell_frac[]) const; // false if out of bounds
		double ValueAt(const unsigned int map_cell[], const double cell_frac[], const SeaData data[]) const;
};

#endif
