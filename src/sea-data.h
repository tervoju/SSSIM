#ifndef _sea_data_h
#define _sea_data_h


#define  SEA_DATA_FILEPATH_DEPTH  "data/baldep.cdf"

#define  SEA_DATA_DIR  "data/2008_08/"
#define  SEA_DATA_FILE_SALT  "salinity.nc"
#define  SEA_DATA_FILE_TEMP  "temperature.nc"
#define  SEA_DATA_FILE_U_VEL  "u_velocity.nc"
#define  SEA_DATA_FILE_V_VEL  "v_velocity.nc"
#define  SEA_DATA_FILE_W_VEL  "w_velocity.nc"


#define  SEA_XVAR  "X66_82"
#define  SEA_YVAR  "Y53_69"
#define  SEA_TVAR  "T"
#define  SEA_ZVAR  "Z"


// variable data horizontal ranges (degrees):
//   x (lon) start 22.4 E, increment 0.2
//   y (lat) start 59.45 N, increment 0.1
//   z layers, in meters:
//     155.9, 125.9, 95.9, 77.9, 71.9, 65.9, 59.9, 53.9, 47.9, 41.9, 35.9, 29.9, 23.9, 19.25, 15.95, 12.65, 10, 8, 6, 4, 1.5

#define  SEA_NX_MAX  17
#define  SEA_NY_MAX  17

#define  SEA_IX  0
#define  SEA_IY  0
#define  SEA_NX  16
#define  SEA_NY  16
#define  SEA_NZ  21
#define  SEA_NT  124

#define  SEA_DEPTH_MAX_X  640
#define  SEA_DEPTH_MAX_Y  740

#define  SEA_DEPTH_NX  93
#define  SEA_DEPTH_NY  93


// salinity range
#define  SEA_SALT_MIN  5.56589
#define  SEA_SALT_MAX  10.8086

// potential_temperature range (degC)
#define  SEA_TEMP_MIN  3.89419
#define  SEA_TEMP_MAX  22.9553

// U range (m/s)
#define  SEA_FLOW_U_MIN  -0.681757
#define  SEA_FLOW_U_MAX  0.496404

// V range (m/s)
#define  SEA_FLOW_V_MIN  -0.498431
#define  SEA_FLOW_V_MAX  0.625208

// W range (m/s)
#define  SEA_FLOW_W_MIN  -0.000535044
#define  SEA_FLOW_W_MAX  0.000431278


#endif
