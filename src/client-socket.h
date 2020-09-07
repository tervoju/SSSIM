#ifndef _sim_socket_h
#define _sim_socket_h

/* requires:
	#include <cstdio>
	#include <unistd.h>
	#include <netinet/in.h>

	#include "udp.h"
*/

class SimSocket : public UDPclient
{
  private:
	char data_type;
	timeval data_wait_tv;

	SimSocket();

  public:
	static timeval default_wait_tv;

	SimSocket(const sockaddr_in *addr) : UDPclient(addr), data_type(0x00), data_wait_tv(default_wait_tv) {}

	ssize_t send(char msg_type, const void *msg_body = NULL, size_t body_len = 0, int flags = 0) const;

	void set_metadata(char type, const timeval wait_tv);
	int get_data(void *buf, ssize_t len, char type = 0, const void *request_body = NULL, size_t request_body_len = 0);
};

#endif
