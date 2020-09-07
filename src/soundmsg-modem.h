#ifndef _soundmsg_modem_h
#define _soundmsg_modem_h


#define SOUNDMODEM_PING_GAP   10  // seconds between ping & first message
#define SOUNDMODEM_TX_MSG_GAP  2  // seconds between message sends within timeslot

class SoundModem {
	private:
		pthread_t thread_id;
		char next_msg_id;

		SoundModem(const SoundModem & _);
		SoundModem & operator= (const SoundModem & _);

	public:
		msg_queue tx_queue;
		trigger_t enabled;

		SoundModem();

		void * thread();
		static void * thread_wrap(void * context) { return reinterpret_cast<SoundModem *>(context)->thread(); }

		bool tx(SoundMsg & msg);
		bool tx(char type, const void * data = NULL, ssize_t data_len = -1);
		int rx(SoundMsg * msg); // returns <0 on error, 0 for no messages, 1 for got message
};

#endif
