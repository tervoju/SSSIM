#ifndef _env_clients_h
#define _env_clients_h


class msg_t {
	public:
		simtime_t rx_time;
		size_t len;
		char * cdata;

		msg_t(simtime_t _rx_time = 0, size_t _len = 0, const char * _cdata = NULL):
			rx_time(_rx_time),
			len(_len),
			cdata(NULL)
		{
			if (len > 0) {
				cdata = new char[len];
				memcpy(cdata, _cdata, len);
			}
		}

		msg_t(const msg_t & m2):
			rx_time(m2.rx_time),
			len(m2.len),
			cdata(NULL)
		{
			if (len > 0) {
				cdata = new char[len];
				memcpy(cdata, m2.cdata, len);
			}
		}

		msg_t & operator= (const msg_t & m2) {
			rx_time = m2.rx_time;
			if ((cdata != m2.cdata) && (m2.len > 0)) {
				len = m2.len;
				delete[] cdata;
				cdata = new char[len];
				memcpy(cdata, m2.cdata, len);
			}
			return *this;
		}

		~msg_t() { delete[] cdata; }
};


enum T_env_client_type { UNDEFINED, FLOAT, BASE, LOG };
class T_env_client {
	private:
		void _sleep(const simtime_t t);
		std::list<msg_t> messages;

	public:
		sockaddr_in addr;
		enum T_env_client_type type;
		SimFloat * simfloat;

		simtime_t wakeup_time;

		void wakeup();
		void sleep(const simtime_t t);

		void queue_message(size_t len, const char * cdata, simtime_t transit_time);
		void handle_messages();

		T_env_client(const sockaddr_in _addr, const char _type);	
		T_env_client(const T_env_client & c2);
		T_env_client & operator= (const T_env_client & c2);
};
typedef std::vector< T_env_client > T_env_client_vector;

int16_t new_client(const sockaddr_in * addr, const char type);

bool valid_client(int16_t id, T_env_client_type type = UNDEFINED);
int16_t base_client_id();

#endif
