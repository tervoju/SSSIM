#include <stdio.h>
// #include "modules_control.h"

//ifdef USE_KALMAN

//#include "lib_common.h"
//#include "lib\debug\debug.h"
#include "kalman_filter.h"

#define UNIT_NOT_ON_SURFACE 0
#define UNIT_ON_SURFACE 1

// have to be changed in 2 places!!!!!!!!!!!!!!!!!!!!!
#define n 6 // amount of robots ONLY IDIOT USES n as define name
#define b (6 * (n))
#define s ((n) * ((n)-1) / 2 + 3 * (n))

//defines to remove keil dependencies
#define os_get_block malloc
#define pool_120 120
#define pool_600 600
#define pool_3000 3000
#define pool_3600 3600

//double kalmandebug[100];
//double saved_X[50];

void run_kalman(kalman_data *K)
{
	//	int i;

	//	kalmandebug[0] = 2;
	//printf("start  run kalman");
	set_measurement_noise(K,
						  K->MeasNoise.this_unit,
						  K->MeasNoise.GPS_error,
						  K->MeasNoise.depth_error,
						  K->MeasNoise.distance_error,
						  K->MeasNoise.unknown_value,
						  K->MeasNoise.smallest_number);

	swatrix_calculate_X_priori(K->X, K->num_units);

	//	for (i=0; i<30; i++)
	//		saved_X[i] = K->X[i];

	swatrix_AxPxATpQ(K->temp_matrix, K->P, K->temp_vector, K->Q_vector, K->X_length, K->num_units, K->delta_t);

	swatrix_copy(K->temp_matrix, K->P, K->X_length * K->X_length);

	swatrix_calculate_C_diagonal(K, K->C, K->X);
	swatrix_calculate_jacob_matrix(K->C, K->X, K->num_units);

	swatrix_calculate_K(K->K, K->P, K->temp_vector, K->temp_matrix, K->R_vector, K->C, K->X_length);

	swatrix_model_measurement(K, 1);

	swatrix_calculate_X_posteriori(K->X, K->K, K->Y, K->H, K->X_length, K->Y_length);

	swatrix_calculate_PmKxCxP(K->temp_matrix, K->P, K->X_length, K->K, K->Y_length, K->temp_vector, K->C);
	swatrix_copy(K->temp_matrix, K->P, K->X_length * K->X_length);
	//printf("kalman run done");
}

void read_kalman_positions(kalman_data *K, kalman_position_data *position_vector)
{
	int i;

	for (i = 0; i < K->num_units; i++)
	{
		position_vector[i].x = K->X[i * 3];
		position_vector[i].y = K->X[i * 3 + 1];
		position_vector[i].z = K->X[i * 3 + 2];
	}
}

void read_kalman_currents(kalman_data *K, kalman_position_data *current_vector)
{
	int i;

	for (i = 0; i < K->num_units; i++)
	{
		current_vector[i].x = K->X[K->num_units * 3 + i * 3];
		current_vector[i].y = K->X[K->num_units * 3 + i * 3 + 1];
		current_vector[i].z = K->X[K->num_units * 3 + i * 3 + 2];
	}
}

void initialize_kalman(kalman_data *Kalm, int number_of_units, MTYP sampling_interval)
{
	//	kalmandebug[0] = 2;
	//	kalmandebug[1] = 2;

	Kalm->num_units = number_of_units;
	Kalm->delta_t = sampling_interval;

	swatrix_allocate(number_of_units, &(Kalm->C), &(Kalm->X), &(Kalm->P), &(Kalm->K), &(Kalm->Q_vector),
					 &(Kalm->R_vector), &(Kalm->Y), &(Kalm->H), &(Kalm->temp_matrix), &(Kalm->temp_vector),
					 &(Kalm->X_length), &(Kalm->Y_length));
}

void end_kalman(kalman_data *Kalm)
{
	/*
	swatrix_free(&(Kalm->C), &(Kalm->X), &(Kalm->P), &(Kalm->K), &(Kalm->Q_vector), 
				 &(Kalm->R_vector), &(Kalm->Y), &(Kalm->H), &(Kalm->temp_matrix), &(Kalm->temp_vector));
*/
}

void position_measurement(kalman_data *K, int this_unit, int is_on_surface, MTYP x, MTYP y, MTYP z)
{
	if (is_on_surface != 0)
	{
		K->IsOnSurface[this_unit] = 1;
		K->Y[this_unit * 3] = x;
		K->Y[this_unit * 3 + 1] = y;
		K->Y[this_unit * 3 + 2] = 0;
	}
	else
	{
		K->IsOnSurface[this_unit] = 0;
		K->Y[this_unit * 3] = 0;
		K->Y[this_unit * 3 + 1] = 0;
		K->Y[this_unit * 3 + 2] = z;
	}
}

void distance_measurement(kalman_data *K, int this_unit, int other_unit, MTYP distance)
{
	int smaller;
	int bigger;
	int index;

	if (this_unit < other_unit)
	{
		smaller = this_unit;
		bigger = other_unit;
	}
	else
	{
		smaller = other_unit;
		bigger = this_unit;
	}

	if (this_unit != other_unit)
	{
		index = smaller * (K->num_units * 2 - smaller - 3) / 2 + bigger - 1;

		//		printf("DISTANCE MEASUREMENT BETWEEN %d AND %d (index=%d) = %.5f \n", smaller, bigger, index, distance);
		K->Y[index + K->num_units * 3] = distance;
	}
}

void set_model_noise(kalman_data *K, MTYP position_error, MTYP depth_error, MTYP current_error, MTYP smallest_number)
{
	int unit;
	MTYP factor = 0.08333;

	for (unit = 0; unit < K->num_units; unit++)
	{
		K->Q_vector[unit * 3] = factor * position_error * position_error;
		K->Q_vector[unit * 3 + 1] = factor * position_error * position_error;
		K->Q_vector[unit * 3 + 2] = factor * depth_error * depth_error;
	}

	for (unit = K->num_units; unit < K->num_units * 2; unit++)
	{
		K->Q_vector[unit * 3] = factor * current_error * current_error;
		K->Q_vector[unit * 3 + 1] = factor * current_error * current_error;
		K->Q_vector[unit * 3 + 2] = factor * current_error * current_error;
	}

	for (unit = 0; unit < K->X_length; unit++)
	{
		if (K->Q_vector[unit] < smallest_number)
			K->Q_vector[unit] = smallest_number;
	}
}

void set_measurement_noise(kalman_data *K, const int this_unit, MTYP GPS_error, MTYP depth_error, MTYP distance_error, MTYP unknown_value, MTYP smallest_number)
{
	int unit1, unit2, row;
	MTYP factor = 0.08333;

	for (unit1 = 0; unit1 < K->num_units; unit1++)
	{
		if (K->IsOnSurface[unit1] == 1)
		{
			K->R_vector[unit1 * 3] = factor * GPS_error * GPS_error;
			K->R_vector[unit1 * 3 + 1] = factor * GPS_error * GPS_error;
			K->R_vector[unit1 * 3 + 2] = factor * depth_error * depth_error;
		}
		else
		{
			K->R_vector[unit1 * 3] = factor * unknown_value * unknown_value;
			K->R_vector[unit1 * 3 + 1] = factor * unknown_value * unknown_value;
			K->R_vector[unit1 * 3 + 2] = factor * depth_error * depth_error;
		}
	}

	row = K->num_units * 3;

	for (unit1 = 0; unit1 < K->num_units - 1; unit1++)
		for (unit2 = unit1 + 1; unit2 < K->num_units; unit2++)
		{
			if (unit1 == this_unit || unit2 == this_unit || this_unit == -1)
				K->R_vector[row] = factor * distance_error * distance_error;
			else
				K->R_vector[row] = factor * unknown_value * unknown_value;

			row++;
		}

	for (row = 0; row < K->Y_length; row++)
	{
		if (K->R_vector[row] < smallest_number)
			K->R_vector[row] = smallest_number;
	}

	K->MeasNoise.this_unit = this_unit;
	K->MeasNoise.GPS_error = GPS_error;
	K->MeasNoise.depth_error = depth_error;
	K->MeasNoise.distance_error = distance_error;
	K->MeasNoise.unknown_value = unknown_value;
	K->MeasNoise.smallest_number = smallest_number;
}

void set_unit_position(kalman_data *K, const int this_unit, const MTYP x, const MTYP y, const MTYP z)
{
	K->X[this_unit * 3] = x;
	K->X[this_unit * 3 + 1] = y;
	K->X[this_unit * 3 + 2] = z;
}

void set_current(kalman_data *K, const int this_unit, const MTYP x, const MTYP y, const MTYP z)
{
	K->X[this_unit * 3 + K->num_units * 3] = x;
	K->X[this_unit * 3 + 1 + K->num_units * 3] = y;
	K->X[this_unit * 3 + 2 + K->num_units * 3] = z;
}

void swatrix_model_measurement(kalman_data *K, MTYP surface_threshold)
{
	//void distance_measurement(kalman_data * K, int this_unit, int other_unit, MTYP distance)
	//void position_measurement(kalman_data * K, int this_unit, int is_on_surface, MTYP x, MTYP y, MTYP z)

	int unit, unit1, unit2, row;

	for (unit = 0; unit < K->num_units; unit++)
	{
		if (K->IsOnSurface[unit] == 1)
		{
			K->H[unit * 3] = K->X[unit * 3];
			K->H[unit * 3 + 1] = K->X[unit * 3 + 1];
			K->H[unit * 3 + 2] = 0;
		}
		else
		{
			K->H[unit * 3] = 0;
			K->H[unit * 3 + 1] = 0;
			K->H[unit * 3 + 2] = K->X[unit * 3 + 2];
		}
	}

	row = K->num_units * 3;

	for (unit1 = 0; unit1 < K->num_units - 1; unit1++)
		for (unit2 = unit1 + 1; unit2 < K->num_units; unit2++)
		{
			K->H[row] = swatrix_calculate_distance(K->X[unit1 * 3] - K->X[unit2 * 3],
												   K->X[unit1 * 3 + 1] - K->X[unit2 * 3 + 1],
												   K->X[unit1 * 3 + 2] - K->X[unit2 * 3 + 2]);

			row++;
		}
}

void swatrix_calculate_PmKxCxP(MTYP *target,
							   MTYP *P, const int P_cols,
							   MTYP *K, const int K_rows,
							   MTYP *tempvector,
							   const C_matr C)
{
	int target_index;
	int first_row_index;
	int com_row_index;
	int row_1;
	int col_2, col_3;
	int com;

	first_row_index = 0;
	target_index = 0;

	for (row_1 = 0; row_1 < K_rows; row_1++)
	{

		for (col_2 = 0; col_2 < C.cols; col_2++)
		{
			tempvector[col_2] = 0;

			for (com = 0; com < C.rows; com++)
				tempvector[col_2] +=
					swatrix_multiply_C_component(K[com + first_row_index], com, col_2, C);
		}

		for (col_3 = 0; col_3 < P_cols; col_3++)
		{
			target[target_index] = P[target_index];

			com_row_index = 0;

			for (com = 0; com < C.cols; com++)
			{
				target[target_index] -= tempvector[com] * P[com_row_index + col_3];
				com_row_index += P_cols;
			}

			target_index++;
		}

		first_row_index += C.rows;
	}
}

void swatrix_calculate_X_posteriori(MTYP *X, MTYP *K, MTYP *Y, MTYP *H, int X_length, int Y_length)
{
	int col, row, K_index;

	K_index = 0;

	for (row = 0; row < X_length; row++)
	{
		for (col = 0; col < Y_length; col++)
		{
			X[row] += K[K_index] * (Y[col] - H[col]);
			K_index++;
		}
	}
}

void swatrix_calculate_X_priori(MTYP *X, int num_units)
{
	int i;

	for (i = 0; i < num_units * 3; i++)
	{
		X[i] = X[i] + X[i + num_units * 3];
	}
}

void swatrix_copy(MTYP *source, MTYP *target,
				  const int arraysize)
{
	int i;

	for (i = 0; i < arraysize; i++)
		target[i] = source[i];
}

void *swatrix_malloc(size_t array_size, const char *arrayname)
{
	unsigned long i;
	void *void_pointer;
	unsigned char *char_pointer;

	//     printf("array: %d %d %d!\n", array_size, sizeof(double),  sizeof(float));
	/*
	if ((array_size == 3 * 4 * n) || (array_size == 4 * s) || (array_size == 4 * b))
	{
		//printf("array size is %d, available blocks in pool: %d\n", array_size, os_check_pool(pool_120));
		printf("array size is %d", array_size);
		if ((void_pointer = os_get_block(pool_120)) == 0)
		{
			printf("error initial pool\n");
		}
		else
		{
			printf("pool initialised\n");
		}
	}
	else if (array_size == (4 * 3 * n * n * (n - 1) / 2))
	{
		//printf("array size is %d, available blocks in pool: %d\n", array_size, os_check_pool(pool_600));
			printf("array size is %d", array_size);
		if ((void_pointer = os_get_block(pool_600)) == 0)
		{
			printf("error initial pool\n");
		}
		else
		{
			printf("pool initialised\n");
		}
	}
	else if (array_size == 4 * b * s)
	{
		//printf("array size is %d, available blocks in pool: %d\n", array_size, os_check_pool(pool_3000));
			printf("array size is %d", array_size);
		if ((void_pointer = os_get_block(pool_3000)) == 0)
		{
			printf("error initial pool\n");
		}
		else
		{
			printf("pool initialised\n");
		}
	}
	else if (array_size == 4 * b * b)
	{
		//printf("array size is %d, available blocks in pool: %d\n", array_size, os_check_pool(pool_3600));
			printf("array size is %d", array_size);
		if ((void_pointer = os_get_block(pool_3600)) == 0)
		{
			printf("error initial pool\n");
		}
		else
		{
			printf("pool initialised\n");
		}
	}
	else
	{
		printf("ERROR! Array %s size of the block is strange: %lu!\n", arrayname, array_size);
		return NULL;
	}
*/
	// this for linux, no need for  complex os_block_alloc etc.
	void_pointer = malloc(array_size);

	if (void_pointer == NULL)
	{
		printf("Memory allocation error\n");
		return NULL;
	}

	char_pointer = (unsigned char *)void_pointer;

	for (i = 0; i < array_size; i++)
		char_pointer[i] = 0;

	return void_pointer;
}

void swatrix_allocate(int num_units,
					  C_matr *C, MTYP **X, MTYP **P, MTYP **K, MTYP **Q_vector, MTYP **R_vector,
					  MTYP **Y, MTYP **H, MTYP **temp_matrix, MTYP **temp_vector,
					  int *X_length, int *Y_length)
{
	int big_dimension = 6 * num_units;
	int small_dimension = (num_units) * (num_units - 1) / 2 + 3 * num_units;

	*X = (MTYP *)swatrix_malloc(big_dimension * sizeof(MTYP), "X");

	*P = (MTYP *)swatrix_malloc(big_dimension * big_dimension * sizeof(MTYP), "P");
	*K = (MTYP *)swatrix_malloc(big_dimension * small_dimension * sizeof(MTYP), "K");
	*Q_vector = (MTYP *)swatrix_malloc(big_dimension * sizeof(MTYP), "Q");
	*R_vector = (MTYP *)swatrix_malloc(small_dimension * sizeof(MTYP), "R");
	*Y = (MTYP *)swatrix_malloc(small_dimension * sizeof(MTYP), "Y");
	*H = (MTYP *)swatrix_malloc(small_dimension * sizeof(MTYP), "H");
	*temp_matrix = (MTYP *)swatrix_malloc(big_dimension * big_dimension * sizeof(MTYP), "temp_matrix");
	*temp_vector = (MTYP *)swatrix_malloc(big_dimension * sizeof(MTYP), "temp_vector");

	C->jacob_cols = 3 * num_units;
	C->jacob_rows = (num_units) * (num_units - 1) / 2;
	C->jacob_matrix = (MTYP *)swatrix_malloc(C->jacob_cols * C->jacob_rows * sizeof(MTYP), "jacob");

	C->diag_length = 3 * num_units;
	C->diag_vector = (MTYP *)swatrix_malloc(C->diag_length * sizeof(MTYP), "diag");

	C->rows = small_dimension;
	C->cols = big_dimension;

	*X_length = big_dimension;
	*Y_length = small_dimension;
}

/*
void swatrix_free(C_matr * C, MTYP ** X, MTYP ** P, MTYP ** K, MTYP ** Q_vector, MTYP ** R_vector,
				  MTYP ** Y, MTYP ** H, MTYP ** temp_matrix, MTYP ** temp_vector)
{
//	free(*X);
//	free(*P);
//	free(*K);
//	free(*Q_vector);
//	free(*R_vector);
//	free(*Y);
//	free(*H);
//	free(*temp_matrix);
//	free(*temp_vector);

//	free(C->jacob_matrix);
//	free(C->diag_vector);
}
*/

void swatrix_inverse(MTYP *source, MTYP *target,
					 const int rows)
{
	int primrow, secrow, col;
	int primrowindx, secrowindx;
	MTYP factor;

	int i, j, k;

	k = 0;

	for (i = 0; i < rows; i++)
		for (j = 0; j < rows; j++)
		{
			target[k] = (i == j ? 1 : 0);

			k++;
		}

	for (primrow = 0, primrowindx = 0; primrow < rows; primrow++, primrowindx += rows)
	{
		factor = source[primrowindx + primrow];

		for (col = 0; col < rows; col++)
		{
			source[primrowindx + col] /= factor;
			target[primrowindx + col] /= factor;
		}

		for (secrow = 0, secrowindx = 0; secrow < rows; secrow++, secrowindx += rows)
			if (secrow != primrow)
			{
				factor = source[secrowindx + primrow];

				for (col = 0; col < rows; col++)
				{
					source[secrowindx + col] -= source[primrowindx + col] * factor;
					target[secrowindx + col] -= target[primrowindx + col] * factor;
				}
			}
	}
}

void swatrix_AxPxATpQ(MTYP *target,
					  MTYP *P,
					  MTYP *tempvector,
					  MTYP *Q_vector,
					  const int A_rows,
					  const int number_of_units, const MTYP delta_t)
{
	int target_index;
	int com_row_index;
	int row_1;
	int col_2, col_3;
	int com;

	target_index = 0;

	for (row_1 = 0; row_1 < A_rows; row_1++)
	{

		for (col_2 = 0; col_2 < A_rows; col_2++)
		{
			tempvector[col_2] = 0;

			com_row_index = 0;

			for (com = 0; com < A_rows; com++)
			{
				tempvector[col_2] +=
					swatrix_multiply_A_component(P[col_2 + com_row_index], row_1, com,
												 number_of_units, delta_t);

				com_row_index += A_rows; // first_cols = second_rows
			}
		}

		for (col_3 = 0; col_3 < A_rows; col_3++)
		{
			target[target_index] = swatrix_return_diagonal_component(row_1, col_3, Q_vector);

			for (com = 0; com < A_rows; com++)
				target[target_index] +=
					swatrix_multiply_A_component(tempvector[com], col_3, com, number_of_units, delta_t);

			target_index++;
		}
	}
}

void swatrix_CxPxCTpR(MTYP *target,
					  MTYP *P,
					  MTYP *tempvector,
					  MTYP *R_vector,
					  const C_matr C, const int P_cols)
{
	int target_index;
	int com_row_index;
	int row_1;
	int col_2, col_3;
	int com;

	target_index = 0;

	for (row_1 = 0; row_1 < C.rows; row_1++)
	{

		for (col_2 = 0; col_2 < P_cols; col_2++)
		{
			tempvector[col_2] = 0;

			com_row_index = 0;

			for (com = 0; com < C.cols; com++)
			{
				tempvector[col_2] +=
					swatrix_multiply_C_component(P[col_2 + com_row_index], row_1, com, C);

				com_row_index += P_cols; // first_cols = second_rows
			}
		}

		for (col_3 = 0; col_3 < C.rows; col_3++)
		{
			target[target_index] = swatrix_return_diagonal_component(row_1, col_3, R_vector);

			for (com = 0; com < C.cols; com++)
				target[target_index] +=
					swatrix_multiply_C_component(tempvector[com], col_3, com, C);

			target_index++;
		}
	}
}

void swatrix_PxCTxM(MTYP *target,
					MTYP *P,
					MTYP *tempvector,
					const C_matr C, const int P_rows, MTYP *M, const int M_cols)
{
	int target_index;
	int first_row_index;
	int com_row_index;
	int row_1;
	int col_2, col_3;
	int com;

	target_index = 0;
	first_row_index = 0;

	for (row_1 = 0; row_1 < P_rows; row_1++)
	{

		for (col_2 = 0; col_2 < C.rows; col_2++)
		{
			tempvector[col_2] = 0;

			for (com = 0; com < C.cols; com++)
			{
				tempvector[col_2] +=
					swatrix_multiply_C_component(P[com + first_row_index], col_2, com, C);
			}
		}

		for (col_3 = 0; col_3 < M_cols; col_3++)
		{
			target[target_index] = 0;

			com_row_index = 0;

			for (com = 0; com < C.rows; com++)
			{
				target[target_index] += tempvector[com] * M[com_row_index + col_3];
				com_row_index += M_cols;
			}

			target_index++;
		}

		first_row_index += C.cols;
	}
}

void swatrix_calculate_K(MTYP *K, MTYP *P, MTYP *tempvector, MTYP *tempmatrix, MTYP *R_vector,
						 const C_matr C, const int P_cols)
{
	swatrix_CxPxCTpR(K, P, tempvector, R_vector, C, P_cols);

	swatrix_inverse(K, tempmatrix, C.rows);
	//	swatrix_print("", "%4.1f", tempmatrix, C.rows, C.rows);

	swatrix_PxCTxM(K, P, tempvector, C, P_cols, tempmatrix, C.rows);
}

MTYP swatrix_multiply_A_component(const MTYP number, const int row, const int column,
								  const int num_units, const MTYP delta_t)
{
	if (row == column)
		return number;
	if (row == column - num_units * 3)
		return number * delta_t;

	return 0;
}

MTYP swatrix_multiply_C_component(const MTYP number, const int row, const int column,
								  const C_matr C)
{
	if (column >= C.diag_length)
		return 0;

	if (row == column)
		return number * C.diag_vector[row];

	if (row < C.diag_length)
		return 0;

	return number * C.jacob_matrix[(row - C.diag_length) * C.jacob_cols + column];
}

MTYP swatrix_return_diagonal_component(const int row, const int column, MTYP *diag_vector)
{
	if (column != row)
		return 0;

	return diag_vector[row];
}

void swatrix_calculate_jacob_matrix(C_matr C, MTYP *x, const int numunits)
{
	int unit1, unit2, unit3;
	int index;
	MTYP distance;

	index = 0;

	for (unit1 = 0; unit1 < numunits - 1; unit1++)
		for (unit2 = unit1 + 1; unit2 < numunits; unit2++)
		{
			distance = swatrix_calculate_distance(x[unit1 * 3] - x[unit2 * 3],
												  x[unit1 * 3 + 1] - x[unit2 * 3 + 1],
												  x[unit1 * 3 + 2] - x[unit2 * 3 + 2]);

			for (unit3 = 0; unit3 < numunits; unit3++)
			{
				C.jacob_matrix[index] = 0;
				C.jacob_matrix[index + 1] = 0;
				C.jacob_matrix[index + 2] = 0;

				if (unit3 == unit1 && distance != 0)
				{
					C.jacob_matrix[index] = (x[unit1 * 3] - x[unit2 * 3]) / distance;
					C.jacob_matrix[index + 1] = (x[unit1 * 3 + 1] - x[unit2 * 3 + 1]) / distance;
					C.jacob_matrix[index + 2] = (x[unit1 * 3 + 2] - x[unit2 * 3 + 2]) / distance;
				}

				if (unit3 == unit2 && distance != 0)
				{
					C.jacob_matrix[index] = (x[unit2 * 3] - x[unit1 * 3]) / distance;
					C.jacob_matrix[index + 1] = (x[unit2 * 3 + 1] - x[unit1 * 3 + 1]) / distance;
					C.jacob_matrix[index + 2] = (x[unit2 * 3 + 2] - x[unit1 * 3 + 2]) / distance;
				}

				index += 3;
			}
		}
}

void swatrix_calculate_C_diagonal(kalman_data *K, C_matr C, MTYP *x)
{
	int unit;

	for (unit = 0; unit < C.diag_length; unit += 3)
	{
		if (K->IsOnSurface[unit])
		{
			C.diag_vector[unit] = 1;
			C.diag_vector[unit + 1] = 1;
			C.diag_vector[unit + 2] = 0;
		}
		else
		{
			C.diag_vector[unit] = 0;
			C.diag_vector[unit + 1] = 0;
			C.diag_vector[unit + 2] = 1;
		}
	}
}

MTYP swatrix_calculate_distance(const MTYP x, const MTYP y, const MTYP z)
{
	return sqrt(x * x + y * y + z * z);
}

//endif //USE_KALMAN
