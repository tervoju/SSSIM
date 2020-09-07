#ifndef _udp_h
#define _udp_h

/* requires:
	#include <cstdio>
	#include <unistd.h>
	#include <netinet/in.h>
*/

#include <cstdio>
#include <unistd.h>
#include <netinet/in.h>

#define PRINT_ERROR_LOCATION fprintf(stderr, "\n%s:%d: %s: ", __FILE__, __LINE__, __func__)
#define PRINT_ERROR(fmt, args...)     \
	do                                \
	{                                 \
		PRINT_ERROR_LOCATION;         \
		fprintf(stderr, fmt, ##args); \
	} while (0)
#define PRINT_PERROR(str)     \
	do                        \
	{                         \
		PRINT_ERROR_LOCATION; \
		perror(str);          \
	} while (0)

bool operator==(const sockaddr_in &x, const sockaddr_in &y);

class inet
{
  public:
	char str[INET_ADDRSTRLEN + 8];
	sockaddr_in addr;

	inet(const sockaddr_in &_addr);
	inet(const char *name, const char *port);
};

class UDPsocket
{
  protected:
	int sockfd;

  public:
	UDPsocket();
	virtual ~UDPsocket() { close(sockfd); }

	unsigned short int port() const;
	ssize_t sendto(const sockaddr_in *dest_addr, const void *buf, size_t len, int flags = 0) const;
	ssize_t sendto(const sockaddr_in *dest_addr, char msg_type, const void *msg_body = NULL, size_t body_len = 0, int flags = 0) const;
	ssize_t recvfrom(sockaddr_in *src_addr, void *buf, size_t len, timeval *tv = NULL, int flags = 0) const;
};

class UDPserver : public UDPsocket
{
  private:
	UDPserver();

  public:
	UDPserver(const char *name_unused, const char *port);
};

class UDPclient : public UDPsocket
{
  private:
	sockaddr_in server_addr;

	UDPclient();

  public:
	UDPclient(const sockaddr_in *addr);

	ssize_t send(const void *buf, size_t len, int flags = 0) const;
	ssize_t recv(void *buf, size_t len, timeval *tv = NULL, int flags = 0)
	{
		return UDPsocket::recvfrom(&server_addr, buf, len, tv, flags);
	}
};

#endif // _udp_h
