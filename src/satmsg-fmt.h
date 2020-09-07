#ifndef _satmsg_fmt_h
#define _satmsg_fmt_h


#define  SATMSG__MAXLEN  1400

class SatMsg {
	private:
		char * ca;

	public:
		class data_err { };

		int16_t from;
		int16_t to;
		uint32_t tx_time;
		uint16_t msg_id;
		uint16_t ack_id;
		uint32_t body_len;

		unsigned char type;
		char * body;

		SatMsg();
		SatMsg(unsigned char _type, const void * _body = NULL, size_t _body_len = 0, int16_t _to = -1);
		SatMsg(const char * data, size_t len);
		SatMsg(const SatMsg & _);

		SatMsg & operator= (const SatMsg & _);
		~SatMsg();

		size_t csize() const;
		char * cdata(size_t prefix = 0);
};

#endif
