#ifndef _client_init_h
#define _client_init_h


/** initialize the base as an env client
 *
 *  sets env_addr & client_id
 *  opens ctrl_socket & log_file
 *  starts ctrl thread: 
 *    receives ctrl messages (sets sim_time or quits on command)
 *    receives gui address (opens gui_socket)
 *    receives sat messages (pushed to msg queue)
 */
void init_base_client(int argc, char **argv);


/** initialize the float as an env client
 *
 *  sets env_addr & client_id
 *  opens ctrl_socket & log_file
 *  starts ctrl thread: 
 *    receives ctrl messages (sets sim_time or quits on command)
 *    receives gui address (opens gui_socket)
 *    receives sat/sound messages (pushed to msg queues)
 *  starts dive thread:
 *    does dive init:
 *      goes to surface
 *      asks base for parameters
 *      dives down & back up
 *      reports back to base
 *    tracks altimeter & CTD (sets bottom_depth, ctd, salt_est & temp_est)
 *    reacts to depth_goal changes
 */
void init_float_client(int argc, char **argv);


#endif
