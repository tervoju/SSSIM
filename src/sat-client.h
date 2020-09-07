#ifndef _sat_client_h
#define _sat_client_h

class SatClient
{
  private:
	pthread_t thread_id;

	simtime_t refresh_time;
	simtime_t connection_mean_time;

	SatClient();

  protected:
	SimSocket skt;

	pthread_mutex_t run_mutex;
	pthread_cond_t run_cond;

	pthread_mutex_t connect_mutex;
	pthread_cond_t connect_cond;

  public:
	bool enabled;
	bool connected;

	SatClient(simtime_t _refresh_time, simtime_t _connection_mean_time);
	virtual ~SatClient();

	virtual void update() = 0; // should broadcast connect_cond & set connected to true

	void *thread();
	static void *thread_wrap(void *context) { return reinterpret_cast<SatClient *>(context)->thread(); }

	bool start();
	bool stop();

	bool wait_for_connection(long timeout_ns = 0);
};

#endif
