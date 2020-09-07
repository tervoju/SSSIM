#ifndef _env_server_h
#define _env_server_h

void udp_send(const sockaddr_in * addr, const void * buf, size_t len);
inline void udp_send(const sockaddr_in * addr, const char byte) { udp_send(addr, &byte, 1); }

void udp_broadcast(const void * buf, size_t len);
inline void udp_broadcast(const char byte) { udp_broadcast(&byte, 1); }

void udp_init();

#endif
