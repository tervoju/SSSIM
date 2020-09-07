#ifndef _util_math_h
#define _util_math_h

/**
 * Smooth interpolation in 0..1, with dv/dx=0 at 0 and 1
 */
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x)))

/**
 * Random number in [-1,1) with triangular distribution with peak at 0
 *
 * uses drand48()
 */
double tri_rand();

/**
 * Random number with normal distribution with mean 0 and std 1
 * generated using Box-Muller transform
 *
 * uses drand48()
 */
double norm_rand();

/**
 * Random number with exponential distribution using rate k = 1
 * for other rates, dive result by k value
 * 
 * mean: 1 / k, median: log(2) / k
 * pdf: k * exp(-k * x), cdf: 1 - exp(-k * x)
 *
 * returns -1.0 for k <= 0
 *
 * uses drand48()
 */
double exponential_rand();

/**
 * Random number >= 1 with Pareto distribution using scale 1 and shape k > 0
 *
 * mean: k / (k - 1) for k > 1, median: pow(2, 1 / k)
 * pdf: k / pow(x, k + 1) for x >= 1, cdf: 1 - pow(x, -k)
 *
 * returns -1.0 for k <= 0
 *
 * uses drand48()
 */
double pareto_rand(double k);

/**
 * Check value v against threshold lim, with approx. sigmoid function
 * probability for values within a transition width (lim-lim_w,lim+lim_w)
 *
 * uses drand48()
 */
bool below_threshold(double v, double lim, double lim_w);

/**
 * Value of a polynomial at x
 *
 * implements the function:
 *   p[0]*(x**n-1) + p[1]*(x**n-2) + ... + p[n-2]*x + p[n-1]
 */
double polyval(const double *p, int n, double x);

/**
 * Variance of X*Y, with independent variables X (x_mean, x_var) and Y (y_mean, y_var)
 *
 * Var(XY) = E[(XY)^2] - E[XY]^2
 *         = E[X^2] * E[Y^2]                                     - E[X]^2 * E[Y]^2
 *         = E[X^2] * E[Y^2] - E[X]^2 * E[Y^2] + E[X]^2 * E[Y^2] - E[X]^2 * E[Y]^2
 *         =                   Var(X) * E[Y^2] + E[X]^2 * Var(Y)
 *         = Var(X) * Var(Y) + Var(X) * E[Y]^2 + E[X]^2 * Var(Y)
 */
double var_mult(double x_mean, double x_var, double y_mean, double y_var);

/**
 * Variance of X/Y, with independent variables X (x_mean, x_var) and Y (y_mean, y_var)
 *
 * Var(X/Y) = E[(X/Y)^2] - E[X/Y]^2
 *          = E[X^2] / E[Y^2]                                     - E[X]^2 / E[Y]^2
 *          = E[X^2] / E[Y^2] - E[X]^2 / E[Y^2] + E[X]^2 / E[Y^2] - E[X]^2 / E[Y]^2
 *          = Var(X) / (Var(Y) + E[Y]^2)        - E[X]^2 *          Var(Y) / (E[Y]^2 * (Var(Y) + E[Y]^2))
 *          = (E[Y]^2 * Var(X) - E[X]^2 * Var(Y)) / (E[Y]^2 * (Var(Y) + E[Y]^2))
 */
double var_div(double x_mean, double x_var, double y_mean, double y_var);

/**
 * Linear interpolation between v0..v1 with position x[0,1]
 */
inline double interpolate(double x, double v0, double v1)
{
	if (x <= 0.0)
		return v0;
	if (x >= 1.0)
		return v1;
	return (1.0 - x) * v0 + x * v1;
}

/**
 * Weighted mean and sample variance of x[] with weights w[]
 * uses algorithm by West (1979)
 *
 * \param[in] values
 * \param[in] weights
 * \param[in] count of value, weight pairs
 * \param[out] mean
 * \param[out] variance
 * \return true if output parameters have been set
 */
//bool weighted_mean_and_variance(double * x, double * w, unsigned int n, double * op_mean, double * op_var);

template <typename Titer1, typename Titer2>
bool weighted_mean_and_variance(Titer1 x, Titer2 w, unsigned int n, double *op_mean, double *op_var)
{
	if (n == 0)
		return false;
	double
		ss = 0.0,
		mean = *x,
		w_sum = *w;
	for (unsigned int i = 1; i < n; ++i, ++x, ++w)
	{
		double prev_w_sum = w_sum;
		w_sum += *w;
		double r = *w * (*x - mean) / w_sum;
		ss += prev_w_sum * r * (*x - mean);
		mean += r;
	}

	*op_mean = mean;
	*op_var = (n == 1) ? 0.0 : ss * n / ((n - 1) * w_sum); // if sample is the population, omit n/(n-1)
	return true;
}

/**
 * Normal distribution PDF
 *
 * based on PROB <http://people.sc.fsu.edu/~jburkardt/cpp_src/prob/prob.html>
 * by John Burkardt (LGPL, 2004)
 */
inline double normal_01_pdf(double x)
{
	// mean 0, variance 1
	static const double norm_factor = 1.0 / sqrt(2.0 * M_PI);
	return exp(-0.5 * x * x) * norm_factor;
}

inline double normal_pdf(double x, double mean, double stdev)
{
	const double y = (x - mean) / stdev;
	return normal_01_pdf(y) / stdev;
}

inline double normal_pdf_inv(double x, double mean, double stdev)
{
	const double y = (x - mean) / stdev;
	return sqrt(2.0 * M_PI) * stdev * exp(0.5 * y * y);
}

#endif
