#ifndef _util_leastsquares_h
#define _util_leastsquares_h

/**
 * Efficient vertical least-squares estimation of v, a variable of t,
 * which has new values added sequentially
 *
 * To use, add() or remove() t,v pairs & read estimate with get_state().
 */
class LeastSquaresTracker {
	public:
		unsigned int n;
		double t0;
	private:
		double s_t, s_t2, s_v, s_v2, s_tv;

		double ss_tt() const { return s_t2 - s_t * s_t / n; }  // sums of squares:    _
		double ss_tv() const { return s_tv - s_t * s_v / n; }  //   SS(v,v) = sum( (v-v)^2 )
		double ss_vv() const { return s_v2 - s_v * s_v / n; }

	public:
		void add(double t, double v);
		void remove(double t, double v);
		void reset();
		bool get_state(double & a, double & b, double & s2) const;  // Expected(v) = a + b * t; s2 is the sample variance
};

#endif // _util_leastsquares_h
