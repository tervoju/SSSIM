/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "udp.h"

// for debug prints

//-----------------------------------------------------------------------------
bool operator==(const sockaddr_in &x, const sockaddr_in &y)
{
	return ((x.sin_port == y.sin_port) && (x.sin_addr.s_addr == y.sin_addr.s_addr));
}

//-----------------------------------------------------------------------------
inet::inet(const sockaddr_in &_addr) : str(), addr(_addr)
{
	if (ntohl(addr.sin_addr.s_addr) == 0x7f000001)
		sprintf(str, "localhost");
	else
		inet_ntop(AF_INET, &addr.sin_addr, str, INET_ADDRSTRLEN);
	uint16_t port = ntohs(addr.sin_port);
	sprintf(str + strlen(str), ":%u", port);
}

inet::inet(const char *name, const char *port) : str(), addr()
{
	str[0] = '\0';

	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (name == NULL)
		hints.ai_flags = AI_PASSIVE;

	addrinfo *servinfo;
	int ra = getaddrinfo(name, port, &hints, &servinfo);
	if (ra != 0)
	{
		PRINT_ERROR("getaddrinfo: %s\n", gai_strerror(ra));
	}
	else
	{
		addr = *reinterpret_cast<sockaddr_in *>(servinfo->ai_addr);
	}
	freeaddrinfo(servinfo);
}

//-----------------------------------------------------------------------------
UDPsocket::UDPsocket() : sockfd(-1)
{
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1)
	{
		PRINT_PERROR("socket");
		exit(1);
	}
}

//-----------------------------------------------------------------------------
unsigned short int UDPsocket::port() const
{
	static unsigned short int port_num = 0;
	if (port_num)
		return port_num;

	sockaddr_in sockinfo;
	socklen_t socklen = sizeof(sockinfo);
	if (getsockname(sockfd, reinterpret_cast<sockaddr *>(&sockinfo), &socklen) == 0)
	{
		port_num = ntohs(sockinfo.sin_port);
	}
	else
	{
		PRINT_PERROR("getsockname");
	}

	return port_num;
}

//-----------------------------------------------------------------------------
ssize_t UDPsocket::sendto(const sockaddr_in *dest_addr, const void *buf, size_t len, int flags) const
{
	socklen_t addrlen = sizeof(sockaddr_in);
	ssize_t tx_bytes = ::sendto(sockfd, buf, len, flags, reinterpret_cast<const sockaddr *>(dest_addr), addrlen);

	//printf(VT_SET(VT_MAGENTA) ">> %u [%zu]" VT_RESET, ntohs(dest_addr->sin_port), len);
	//if (tx_bytes != (int)len) printf(": " VT_SET2(VT_BRIGHT, VT_RED) "ERROR: tx %zi of %zu bytes" VT_RESET, tx_bytes, len);
	//else putchar('\n');

	if (tx_bytes == -1)
		PRINT_PERROR("sendto");
	return tx_bytes;
}

ssize_t UDPsocket::sendto(const sockaddr_in *dest_addr, char msg_type, const void *msg_body, size_t body_len, int flags) const
{
	socklen_t addrlen = sizeof(sockaddr_in);

	ssize_t tx_bytes = -1;
	if ((msg_body == NULL) || (body_len <= 0))
	{
		tx_bytes = ::sendto(sockfd, &msg_type, 1, flags, reinterpret_cast<const sockaddr *>(dest_addr), addrlen);
	}
	else
	{
		char *buf = new char[1 + body_len];
		buf[0] = msg_type;
		memcpy(buf + 1, msg_body, body_len);
		tx_bytes = ::sendto(sockfd, buf, 1 + body_len, flags, reinterpret_cast<const sockaddr *>(dest_addr), addrlen);
		delete[] buf;
	}

	//printf(VT_SET(VT_MAGENTA) ">> %u %s" VT_SET(VT_DIM) " [%zu]" VT_RESET, ntohs(dest_addr->sin_port), msg_name(msg_type), 1 + body_len);
	//if (tx_bytes != 1 + (int)body_len) printf(": " VT_SET2(VT_BRIGHT, VT_RED) "ERROR: tx %zi of %zu bytes\n" VT_RESET, tx_bytes, 1 + body_len);
	//else putchar('\n');

	if (tx_bytes == -1)
		PRINT_PERROR("sendto");
	return tx_bytes;
}

//-----------------------------------------------------------------------------
ssize_t UDPsocket::recvfrom(sockaddr_in *src_addr, void *buf, size_t len, timeval *tv, int flags) const
{
	if (tv != NULL)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		int s = select(sockfd + 1, &readfds, NULL, NULL, tv);
		if (s < 0)
		{
			PRINT_PERROR("select");
			return s;
		}

		if (s == 0)
			return 0; // no messages before timeout
	}

	socklen_t addrlen = sizeof(sockaddr_in);
	ssize_t rx_bytes = ::recvfrom(sockfd, buf, len, flags, reinterpret_cast<sockaddr *>(src_addr), &addrlen);
	if (rx_bytes == -1)
		PRINT_PERROR("recvfrom");
	return rx_bytes;
}

//-----------------------------------------------------------------------------
UDPserver::UDPserver(const char *name_unused, const char *port) : UDPsocket()
{
	const sockaddr_in addr = inet(NULL, port).addr;
	if (bind(sockfd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
	{
		PRINT_PERROR("bind");
		exit(1);
	}
}

//-----------------------------------------------------------------------------
UDPclient::UDPclient(const sockaddr_in *addr) : UDPsocket(), server_addr()
{
	if (connect(sockfd, reinterpret_cast<const sockaddr *>(addr), sizeof(sockaddr_in)) == -1)
	{
		PRINT_PERROR("connect");
		close(sockfd);
		exit(1);
	}

	server_addr = *addr;
}

//-----------------------------------------------------------------------------
ssize_t UDPclient::send(const void *buf, size_t len, int flags) const
{
	ssize_t tx_bytes = ::send(sockfd, buf, len, flags);

	if (tx_bytes == -1)
		PRINT_PERROR("send");
	return tx_bytes;
}
