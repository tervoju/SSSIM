#ifndef _dive_ctrl_h
#define _dive_ctrl_h


bool dive_is_stable();

double go_to_surface(double d_surface = 0.0);

void * dive_thread_function(void * unused);


#endif
