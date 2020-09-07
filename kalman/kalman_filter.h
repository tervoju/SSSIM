/*************************************************************************

This library implements a Kalman-filter that is used for navigation of the
SWARM-system. The measurements (distances from unit to unit, GPS-positions
or depths of the units), estimates of the sea currents and the amount of
noise are entered into the filter. The filter calculates the positions of
the units based on these measurements. The positions are estimates based
on a mathematical model.

The Kalman-filter is implemented in ANSI-C, not C++. However, the routines
and data structures are built so that all the data (matrices and
parameters) is wrapped in one data structure. This makes the function
calls as simple as possible. The user does not necessarily need to know
what is happening inside the filter.

The main functions are divided in four categories:

1) Initialization
	The functions in this category initialize the matrices and parameters
	needed by the Kalman-filter. These functions define the initial 
	positions of the units, the estimations of the sea currents in the
	initial position, and the amount of noise in measurements and in 
	model.

2) Putting the measurements into the filter
	These functions insert the real-world measurements into the Kalman-
	filter.

3) Kalman-filter
	This category contains only one function. It runs the filter itself.
	It increases the time by one interval (the size of the interval is
	defined earlier), and updates the estimates.

4) Getting the estimates from the filter
	This part contains the functions that get the estimates of the
	currents and the positions of the units from the Kalman-filter.

In addition there are helper functions. The user does not need these
functions, but they are used to do all the dirty work in the main
functions (matrix operations etc). The helper functions all start with
prefix "swatrix_" (derived from SWARM and matrix).

*************************************************************************/


#include <stdlib.h>
#include <math.h>


// Here we define, which data type we use in matrices. If double consumes
// too much memory, try using float instead.
#define MTYP double



/****************** Data structure definitions **************************/

// C_matr-structure contains the data that describes the C matrix. The C
// matrix is not described cell by cell because most of the matrix is
// filled with zeroes. This structure is wrapped inside kalman_data
// structure. It is needed only in swatrix_ functions (helper-functions),
// so you should not need this.
typedef struct {
	int rows;	// the size of...
	int cols;	// ...the C-matrix

	MTYP * jacob_matrix;	// pointer to the array containing the
							// jacobian-elements of the matrix
	int jacob_rows;	// the size of the...
	int jacob_cols;	// ...jacobian matrix

	MTYP * diag_vector;	// pointer to the array containing the diagonal
						// elements of the C-matrix

	int diag_length;	// the length of the diagonal array
} C_matr;


typedef struct {
	int this_unit;
	MTYP GPS_error;
	MTYP depth_error;
	MTYP distance_error;
	MTYP unknown_value;
	MTYP smallest_number;
} kalman_measurement_noise_type;


// kalman_data structure contains the matrices used by the Kalman-filter.
// It also contains some other necessary information, like number of
// units contained by the system.
typedef struct {
	C_matr C;	// C matrix

	kalman_measurement_noise_type MeasNoise;

	MTYP * X,	// Pointer to state vector
		 * P,	// Pointer to P matrix (the covariances of the estimate)
		 * K,	// Pointer to K matrix (Kalman gain)

		 * Q_vector,	// Pointer to vector containing the diagonal
						// elements of Q matrix (model noise). All the
						// other elements of Q matrix are zeroes.

		 * R_vector,	// Pointer to vector containing the diagonal
						// elements of R matrix (measurement nois).	

		 * Y,	// Pointer to measurement vector

		 * H,	// Pointer to vector containing values from the 
				// measurement equation from the model.

		 * temp_matrix,	// Pointer to temporary matrix needed by some
						// matrix operations

		 * temp_vector;	// Pointer to temporary vector

	int num_units;	// Number of units in the SWARM system
	MTYP delta_t;	// Time interval between two runs of Kalman-filter

	int X_length,	// Length of state vector
		Y_length;	// Length of measurement vector

	char IsOnSurface[10];
} kalman_data;


// kalman_position_data is a simple structure that stores coordinates of
// one SWARM unit (drifter). This same structure is also used to store 
// directional components of sea current affecting to one SWARM unit.
// Basically, you need array of kalman_position_data to get all the
// positions or currents.
typedef struct {
	MTYP x;
	MTYP y;
	MTYP z;
} kalman_position_data;


/******* Main function definitions: Initialization functions ************/

//Initializes the Kalman-filter matrices except the noise matrices.
void initialize_kalman(
	kalman_data * K,		// Pointer to a kalman_data type variable
	int number_of_units,	// How many units are there in the system
	MTYP sampling_interval	// In how big intervals we run the filter
							// (in seconds)
);


// Sets the amount of different kinds of noises in the model. The
// amounts of the noises are given as error ranges (from end to end).
// The function converts these to variances of a rectangular
// distribution. These variances are set as diagonal elements of
// Q matrix.
//
// For example if you know that the accuracy of the positions of the
// units in mathematical model is +-10 meters, you set the
// position_error parameter to 20 (from -10 to +10).
//
// The Kalman-filter needs some inaccuracy for all the variables.
// The smallest_number parameter defines the smallest possible
// variance. This is because if we have zeroes in the diagonal of
// Q matrix, we run into problems. For example, if you set
// smallest_number to 0.0001, this means inaccuracy of about 1cm,
// which should be small enough.
void set_model_noise(
	kalman_data * K,		// Pointer to a kalman_data type variable
	MTYP position_error,	// Error in the position estimates (X and Y)
	MTYP depth_error,		// Error in the depth estimates (Z)

	MTYP current_error,		// Error in the set current estimates
							// (X, Y and Z)

	MTYP smallest_number	// What is the smallest number we can deal
							// with in Q matrix. 0.0001 should be a good
							// value.
);


// Sets the amount of different kinds of noises in the measurements.
// The amounts of the noises are given as error ranges (from end to
// end). The function converts these to proper diagonal elements
// of R matrix.
//
// For example if you know that the accuracy of the GPS measurements
// is +-5 meters, you set the GPS_error to 10 (from -5 to +5).
//
// smallest_number works in same way as in set_model_noise.
//
// The Kalman-filter works in two different ways. The filter can either
// know the distances from all the units to all other units.
// Alternatively, the filter knows only the distances from this unit
// to all the other units, and the rest of the distances are unknown.
// this_unit parameter tells the filter, which is our viewpoint. If you
// enter -1 to this, the filter assumes that it knows all the distances.
// If you enter some other number (for example between 0 and 4, if there
// are 5 units), you define, which is "this" unit. That means that the
// Kalman-filter does not care about the distances between the other
// units.
//
// The implementation of this feature is made in a "dirty" way. The filter
// assumes that it knows all the distance measurements. However, because
// we don't actually know all of them, we have to tell the filter not to
// rely on the data that is not real. In practice, we set the errors of
// the unknown distance measurements to some huge value. This value
// is defined by the unknown_value parameter.
//
// For example, if the program is running in unit 1, and we have 3 units
// (indexed from 0 to 2), we know the distances between 0 and 1, and also
// between 1 and 2, but not between 0 and 2. Therefore, we define that the
// accuracy of the distance measurements 0-1 and 1-2 is distance_error
// (a real inaccuracy in distance measurements, for example 5 meters).
// However, the distance accuracy of the distance measurement 0-2 is
// set to unknown_value (a big number, for example 10000 meters).
void set_measurement_noise(
	kalman_data * K,		// Pointer to a kalman_data type variable
	const int this_unit,	// The number of this unit or -1 (see above)
	MTYP GPS_error,			// Error in GPS measurements
	MTYP depth_error,		// Error in depth measurements
	MTYP distance_error,	// Real error in distance measurements

	MTYP unknown_value,		// Artificial error value of the distance
							// measurements between units, whose distance
							// from each other we actually don't know
							// (explained above). 10000 should be a good
							// value.

	MTYP smallest_number	// What is the smallest number we can deal
							// with in R matrix. 0.0001 should be a good
							// value.
);


// Set the initial position of a unit. You should set the positions of all
// the units to some value.
void set_unit_position(
	kalman_data * K,		// Pointer to a kalman_data type variable
	const int this_unit,	// The number of the unit
	const MTYP x,			// The X-...
	const MTYP y,			// ...Y-...
	const MTYP z			// ...and Z-coordinates of the unit.
);


// Set the initial estimation of the sea current affecting to a unit. You
// should set all the currents to some value.
void set_current(
	kalman_data * K,		// Pointer to a kalman_data type variable
	const int this_unit,	// The number of the unit to which the current
							// is affecting.
	
	const MTYP x,			// X-...
	const MTYP y,			// ...Y- and...
	const MTYP z			// ...Z-components of the current vector
);


// Frees the memory (destroys the matrices).
void end_kalman(
	kalman_data * Kalm		// Pointer to a kalman_data type variable
);


/******* Main function definitions: Measurements into the filter ********/

// We have a measurement of unit's location, either GPS location or depth.
// With this function we let the Kalman-filter know this measurement.
//
// If the unit is on the surface, the filter utilizes the X- and Y-
// coordinates, but sets the Z-coordinate to 0. If the unit is not on the
// surface, the filter does not care about the X- and Y-coordinates (you
// can set them to any value), but only the Z-coordinate (depth) matters.
void position_measurement(
	kalman_data * K,		// Pointer to a kalman_data type variable
	int this_unit,			// The number of the unit
	int is_on_surface,		// Is the unit on the surface (0=NO, 1=YES)
	MTYP x,					// X- and...
	MTYP y,					// ...Y-coordinates of the unit
	MTYP z					// Z-coordinate (=depth) of the unit
);


// Now we have a measurement of a distance between two units. This function
// is used to let the Kalman-filter also know this fact.
void distance_measurement(
	kalman_data * K,	// Pointer to a kalman_data type variable
	int this_unit,		// Number of the first unit
	int other_unit,		// Number of the second unit
	MTYP distance		// The actual distance measurement between these
						// units
);


/******* Main function definitions: Kalman-filtering ********************/

// Runs the Kalman-filter for one round.
void run_kalman(
	kalman_data * K			// Pointer to a kalman_data type variable
);


/******* Main function definitions: Estimates from the filter ***********/

// Reads the estimates of the positions of the units from the Kalman-
// filter and stores them to position_vector.
void read_kalman_positions(
	kalman_data * K,		// Pointer to a kalman_data type variable
	kalman_position_data * position_vector	// Pointer to array of
											// position_vector elements
);


// Reads the estimates of the sea currents from the Kalman-filter and
// stores them to current_vector.
void read_kalman_currents(
	kalman_data * K,		// Pointer to a kalman_data type variable
	kalman_position_data * current_vector	// Pointer to array of
											// current_vector elements
);


/****************** Helper function definitions *************************/

// "Measurement equation"
// This function is used to make "measurements" from the model. This means
// that when the filter is in some state, we use the measurement equation
// to determine the distances and positions of the estimated locations.
// In practice, this function calculates the elements of the H vector.
void swatrix_model_measurement(
	kalman_data * K,		// Pointer to a kalman_data type variable
	MTYP surface_threshold	// How deep Z is still considered as surface.
							// Something like 2 meters might be a good 
							// guess.
);


// Calculate a matrix operation P-K*C*P
void swatrix_calculate_PmKxCxP(
	MTYP * target,		// Pointer to target matrix
	MTYP * P,			// Pointer to P matrix
	const int P_cols,	// Columns in P matrix
	MTYP * K,			// Pointer to K matrix
	const int K_rows,	// Rows in K matrix
	MTYP * tempvector,	// Pointer to temporary vector
	const C_matr C		// C-matrix
);


// Calculate the a-posteriori estimate of X (state vector)
void swatrix_calculate_X_posteriori(
	MTYP * X,		// Pointer to X vector
	MTYP * K,		// Pointer to K matrix
	MTYP * Y,		// Pointer to Y vector (actual measurements)
	MTYP * H,		// Pointer to H vector (model "measurements")
	int X_length,	// X vector length
	int Y_length	// Y vector length
);


// Calculate the a-priori estimate of X (state vector)
void swatrix_calculate_X_priori(
	MTYP * X,		// Pointer to X vector
	int num_units	// Number of units in the SWARM system
);


// Copy a matrix
void swatrix_copy(
	MTYP * source,	// Pointer to the source matrix
	MTYP * target,	// Pointer to the target matrix
	const int arraysize	// Number of elements in the matrix
);


// Allocate memory for a matrix (=array)
void * swatrix_malloc(
	size_t array_size,	// Number of elements in the matrix
	const char * arrayname
);


// Allocate memory for all the matrices and sets the lengths of the
// X and Y vectors.
void swatrix_allocate(
	int num_units,			// Number of units in the SWARM system
	C_matr * C,				// Pointer to variable defining the C matrix
	MTYP ** X,				// Pointer to a pointer of X vector
	MTYP ** P,				// Pointer to a pointer of P matrix
	MTYP ** K,				// Pointer to a pointer of K matrix
	MTYP ** Q_vector,		// Pointer to a pointer of Q diagonal vector
	MTYP ** R_vector,		// Pointer to a pointer of R diagonal vector
	MTYP ** Y,				// Pointer to a pointer of Y vector
	MTYP ** H,				// Pointer to a pointer of H vector
	MTYP ** temp_matrix,	// Pointer to a pointer of temporary matrix
	MTYP ** temp_vector,	// Pointer to a pointer of temporary vector
	
	int * X_length,			// Pointer to a variable defining the length
							// of the X vector

	int * Y_length			// Pointer to a variable defining the length
							// of the Y-vector
);


// Frees the memory
void swatrix_free(
	C_matr * C,				// Pointer to variable defining the C matrix
	MTYP ** X,				// Pointer to a pointer of X vector
	MTYP ** P,				// Pointer to a pointer of P matrix
	MTYP ** K,				// Pointer to a pointer of K matrix
	MTYP ** Q_vector,		// Pointer to a pointer of Q diagonal vector
	MTYP ** R_vector,		// Pointer to a pointer of R diagonal vector
	MTYP ** Y,				// Pointer to a pointer of Y vector
	MTYP ** H,				// Pointer to a pointer of H vector
	MTYP ** temp_matrix,	// Pointer to a pointer of temporary matrix
	MTYP ** temp_vector		// Pointer to a pointer of temporary vector
);


// Calculates the inverse of the matrix
void swatrix_inverse(
	MTYP * source,
	MTYP * target,
	const int rows
);


// Calculates A*P*A'+Q (A' is a transpose of A)
void swatrix_AxPxATpQ(
	MTYP * target,				// Pointer to target matrix
	MTYP * P,					// Pointer to P matrix
	MTYP * tempvector,			// Pointer to temporary vector
	MTYP * Q_vector,			// Pointer to Q diagonal vector
	const int A_rows,			// Number of rows in A
	const int number_of_units,	// Number of units
	const MTYP delta_t			// Sampling interval
);


// Calculate C*P*C'+R (C' is a traspose of C)
void swatrix_CxPxCTpR(
	MTYP * target,			// Pointer to target matrix
	MTYP * P,				// Pointer to P matrix
	MTYP * tempvector,		// Pointer to temporary vector
	MTYP * R_vector,		// Pointer to R diagonal vector
	const C_matr C,			// Variable that defines the C matrix
	const int P_cols		// Columns in P matrix
);


// Calculate P*C'*M (M is an arbitary matrix)
void swatrix_PxCTxM(
	MTYP * target,		// Pointer to target matrix
	MTYP * P,			// Pointer to P matrix
	MTYP * tempvector,	// Pointer to temporary vector
	const C_matr C,		// Variable that defines the C matrix
	const int P_rows,	// Rows in P matrix
	MTYP * M,			// Pointer to M matrix
	const int M_cols	// Columns ins M matrix
);


// Calculate the K
void swatrix_calculate_K(
	MTYP * K,			// Pointer to K matrix
	MTYP * P,			// Pointer to P matrix
	MTYP * tempvector,	// Pointer to temporary vector
	MTYP * tempmatrix,	// Pointer to temporary matrix
	MTYP * R_vector,	// Pointer to R diagonal vector
	const C_matr C,		// Variable that defines the C matrix
	const int P_cols	// Columns in P matrix
);


// Multiply a number with an A matrix element
MTYP swatrix_multiply_A_component(
	const MTYP number,	// Number to be multiplied with the matrix element
	const int row,		// The row number of the element in A matrix
	const int column,	// The column of the A matrix element
	const int num_units,// Number of units in SWARM system
	const MTYP delta_t	// Sampling interval
);


// Multiply a number with a C matrix element
MTYP swatrix_multiply_C_component(
	const MTYP number,	// Number to be multiplied with the matrix element
	const int row,		// The row number of the element in C matrix
	const int column,	// The column of the C matrix element
	const C_matr C		// Variable that defines the C matrix
);


// Return a component from a diagonal matrix. You enter the row and column
// of the matrix element, and the vector containing the diagonal
// elements of the matrix. All other elements of the matrix are considered
// as 0. If row is equal to column, the corresponding element from the
// diagonal vector is returned, otherwise 0 is returned.
MTYP swatrix_return_diagonal_component(
	const int row,		// Row number of the element in the matrix
	const int column,	// Column number of the element in the matrix
	MTYP * diag_vector	// Vector containing the diagonal components
);


// Calculates the jacobian components of the C matrix
void swatrix_calculate_jacob_matrix(
	C_matr C,			// Variable that defines the C matrix
	MTYP * x,			// Pointer to the X vector (state vector)
	const int numunits	// Number of units in the SWARM system
);


// Calculates the diagonal elements of the C matrix
void swatrix_calculate_C_diagonal(
	kalman_data * K,		// Pointer to a kalman_data type variable
	C_matr C, 	// Variable that defines the C matrix
	MTYP * x	// Pointer to the X vector (state vector)
);


// Calculate euclidian distance. We know the differences of X-, Y- and
// Z-components. This is simply a sqrt(x*x+y*y+z*z) function, but just in
// case you want to replace the sqrt function with something else,
// (if you can find a faster way to calculate the distance) just edit this
// function. This is the only place where sqrt is used.
MTYP swatrix_calculate_distance(
	const MTYP x,	// Difference of X-components
	const MTYP y,	// Difference of X-components
	const MTYP z	// Difference of Y-components
);
