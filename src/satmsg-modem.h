#ifndef _satmsg_modem_h
#define _satmsg_modem_h

/* requires:
	#include <string>
	#include <cstdio>
	#include <pthread.h>
	#include <stdint.h>
	#include <unistd.h>
	#include <netinet/in.h>
	
	#include "udp.h"
	#include "client-socket.h"
	#include "satmsg-fmt.h"
*/

struct comms_log_t {
	uint16_t next_msg_id;
	uint16_t next_ack_id;
	uint16_t rx_ack_id;

	comms_log_t(): next_msg_id(1), next_ack_id(0), rx_ack_id(0) { }
};

class SatModem: public SatClient {
	private:
		bool always_connected;
		pthread_mutex_t talk_mutex;
		msg_queue * rx_queue;

		std::map<int16_t, comms_log_t> comms_log;

		int _tx(const char * cdata, size_t len);

		SatModem(const SatModem & sm2);
		SatModem & operator= (const SatModem & sm2);

	public:
		SatModem(msg_queue * _rx_queue, bool _always_connected = false, simtime_t _refresh_time = 60, simtime_t _connection_mean_time = 120);
		~SatModem();

		void update();

		bool tx(int16_t to, char type, const void * data = NULL, ssize_t data_len = -1);
		bool tx(int16_t to, floatmsg_t * msg) { return tx(to, msg->type(), msg->cdata(), msg->csize()); }
		bool tx(int16_t to, basemsg_t * msg) { return tx(to, msg->type(), msg->cdata(), msg->csize()); }
		int rx(SatMsg * msg, long timeout_ns = 0); // returns <0 on error, 0 for no messages, 1 for got message

		int unanswered_messages(int16_t id = -1) const;
};

#endif
