#ifndef _soundmsg_fmt_h
#define _soundmsg_fmt_h

/* requires:
	#include <stdint.h>
*/

const char
	SOUNDMSG_PING = 0x45,         // 0100 0101
	SOUNDMSG_PONG = 0x46,         // 0100 0110
	SOUNDMSG_ASCII = 0x50,        // 0101 0000
	SOUNDMSG_GPS_POS = 0x60,      // 0110 0000
	SOUNDMSG_POS_EST = 0x61;	  // 0110 0001   // JTE ADD

#define  SOUNDMSG_OK               0x01
#define  SOUNDMSG_ERROR_NO_SIGNAL  0xE0
#define  SOUNDMSG_ERROR_BAD_ID     0xE1
#define  SOUNDMSG_ERROR_BAD_DATA   0xE2

#define  SOUNDMSG__ID_UNSET  -1
#define  SOUNDMSG__ID_MODEM  -3


// stream format: from [int16_t], time [uint32_t], type [char], id [char], msg_body

class SoundMsg {
	private:
		char * ca;

	public:
		class data_err { };

		int16_t from;
		uint32_t time;
		char type;
		char id;
		char body_len;

		char * body;

		SoundMsg(char _type = 0, const void * _body = NULL, char _body_len = -1, char _id = -1);
		SoundMsg(const char * data, size_t len);
		SoundMsg(const SoundMsg & _);
		SoundMsg & operator= (const SoundMsg & _);

		~SoundMsg();

		size_t csize() const;
		char * cdata(size_t prefix = 0);
};

#endif
