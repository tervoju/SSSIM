#include "modules_control.h"

#ifdef USE_KALMAN
//
//#include "lib_common.h"
//#include "lib\debug\debug.h"

kalman_data Kalman;

//###############################################################################
// init Kalman
//###############################################################################
void initKalman(){
  int nn = 0;

  for (i=1; i<SWARM_NUM_OF_UNITS_IN_GROUP; i++)
    nn = nn + i;
  // Initialization of the Kalman-filter
  initialize_kalman(&Kalman, SWARM_NUM_OF_UNITS_IN_GROUP, kalman_sampling_interval);

  // Set the amount of noise in measurements
  set_measurement_noise(&Kalman, 0, 10, 0.001, 5, 10000, 0.0001);

  // Set the inaccuracy of the state-space model (treated as white noise)
  set_model_noise(&Kalman, 10, 1, 0.1, 0.0001);

  // Initialize the positions of the units
  set_unit_position(&Kalman, 0, 200, 100, 0);
  set_unit_position(&Kalman, 1, 300, 100, 0);
  set_unit_position(&Kalman, 2, 150, 200, 50);
  set_unit_position(&Kalman, 3, 120, 300, 90);
  set_unit_position(&Kalman, 4, 100, 150, 100);

  // Initialize the sea currents
  set_current(&Kalman, 0, 0, 0, 0);
  set_current(&Kalman, 1, 0, 0, 0);
  set_current(&Kalman, 2, 0, 0, 0);
  set_current(&Kalman, 3, 0, 0, 0);
  set_current(&Kalman, 4, 0, 0, 0);
}


//###############################################################################
  update Kalman
//###############################################################################
void updateKalman(){

/* IMPLEMENT!!!!!!!!!!!!!!!
        position_measurement(&Kalman, i, 1, value_x, value_y, value_z);
      else
        position_measurement(&Kalman, i, 0, value_x, value_y, value_z);

        distance_measurement(&Kalman, i, j, value);

    Kalman.delta_t = KALMAN_SAMPLING_INTERVAL;
*/
}


//###############################################################################
  run Kalman
//###############################################################################
void runKalman(){
    run_kalman(&Kalman);
}


//###############################################################################
  read position values from Kalman
//###############################################################################
void readKalmanPositions(){
/* IMPLEMENT!!!!!!!!!!!!!!!
  void read_kalman_positions(
  kalman_data * K, // Pointer to a kalman_data type variable
  kalman_position_data * position_vector // Pointer to array of position_vector elements
  );
*/
}

//###############################################################################
  read current values from Kalman
//###############################################################################
void readKalmanPositions(){
/* IMPLEMENT!!!!!!!!!!!!!!!
  void read_kalman_currents(
  kalman_data * K, // Pointer to a kalman_data type variable
  kalman_position_data * current_vector // Pointer to array of current_vector elements
  )
*/
}


//###############################################################################
  kill Kalman
//###############################################################################
void killKalman(){
	end_kalman(&Kalman);
}

#endif //USE_KALMAN


