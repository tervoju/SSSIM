#ifndef _util_tsdouble_h
#define _util_tsdouble_h


// thread-safe double using mutex
class ts_double {
	private:
		double v;
		pthread_mutex_t mutex;

	public:
		ts_double(double _v = 0.0);
		~ts_double();

		double get();
		void set(double _v);
};

#endif
