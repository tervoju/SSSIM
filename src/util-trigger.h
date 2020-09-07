#ifndef _util_trigger_h
#define _util_trigger_h

class trigger_t {
	private:
		pthread_mutex_t mutex;
		pthread_cond_t cond_true, cond_false;

		bool value;

	public:
		trigger_t(bool init_value = false);
		~trigger_t();

		bool get() const { return value; }
		void set(bool new_value);
		void wait_for(bool it);
};

#endif
