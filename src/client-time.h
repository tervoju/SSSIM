#ifndef _client_time_h
#define _client_time_h

/* requires:
	#include <pthread.h>
	#include <stdint.h>
	#include <sys/time.h>
*/


//-----------------------------------------------------------------------------
class SimTime {
	private:
		mutable pthread_mutex_t mutex;
		pthread_cond_t run_cond;
		simtime_t time_now;
		void do_sleep_until(const simtime_t t) const;

	public:
		SimTime(); ~SimTime();
		int set(char ctrl, const void * buf, ssize_t len);
		simtime_t now() const { return time_now; }

		cycle_params_t cycle;
		int cycle_number(simtime_t t = 0) const;
		int cycle_time(simtime_t t = 0, bool pre_start_ok = false) const;

		simtime_t sleep_until(const simtime_t wakeup_time);
		simtime_t sleep(int sleep_seconds);

		void enable_sleep();
		void suspend_sleep();
};

#endif
