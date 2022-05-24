#ifndef NOTER_NET_FUNC
#define NOTER_NET_FUNC

#include <sys/types.h>
#include <sys/socket.h>

int sendAll(int s_descr, const char* buff, size_t length);

int recvAll(int s_descr, char *buf_ptr, size_t length, time_t *last_data_exchange_timestamp);

int connectWait(int sock_descr, struct sockaddr *addr, size_t addrlen, struct timeval *timeout);

int setSocketOptions(int server_socket_dscr);

#endif //NOTER_NET_FUNC
