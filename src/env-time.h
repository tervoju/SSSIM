#ifndef _env_time_h
#define _env_time_h


class time_msg_t {
	public:
		char * buf;
		const size_t len;

		time_msg_t(char msg_type = MSG_TIME);
		~time_msg_t() { delete[] buf; }

	private:
		time_msg_t(const time_msg_t & _);
		time_msg_t & operator= (const time_msg_t & _);
};


void print_timestr();

simtime_t time_init(int argc, char **argv);

#endif
