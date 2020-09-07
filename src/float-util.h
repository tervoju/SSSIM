#ifndef _float_util_h
#define _float_util_h


bool          at_surface(SimSocket * env_skt);         bool          at_surface(); 
unsigned char get_echo(SimSocket * env_skt);           unsigned char get_echo(); 
ctd_data_t    get_ctd(SimSocket * env_skt);            ctd_data_t    get_ctd(); 
gps_data_t    get_gps(SimSocket * env_skt);            gps_data_t    get_gps(); 
double        get_float_density(SimSocket * env_skt);  double        get_float_density();

bool set_float_density(double new_density, SimSocket * env_skt);
bool set_float_density(double new_density);

double approx_baltic_density(double depth);

bool print_status();
bool print_envstate();
bool compare_profile();

#endif
