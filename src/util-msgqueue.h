#ifndef _util_msgqueue_h
#define _util_msgqueue_h

/* requires:
	#include <queue>
	#include <string>
*/

typedef std::vector<char> t_msg;

// custom thread-safe wrapper for std::queue
class msg_queue {
	private:
		ssize_t max_length;
		std::queue<t_msg> queue;
		pthread_mutex_t mutex;
		pthread_cond_t push_cond;

		t_msg _get(bool pop);

	public:
		msg_queue(); ~msg_queue();

		void set_max_length(ssize_t _max_length);
		bool empty() const;
		void clear();

		bool push(const char * buf, ssize_t len); // returns false if max_length reached

		int wait_for_msg(long timeout_ns = 0);

		// returns empty string if queue is empty
		t_msg pop() { return _get(true); }
		t_msg peek() { return _get(false); }
};

#endif
