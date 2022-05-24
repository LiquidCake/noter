#ifndef NOTER_SRV_NET_FUNC
#define NOTER_SRV_NET_FUNC

#include <sys/types.h>
#include <sys/socket.h>

int sendAll(int s_descr, const char* buff, size_t length);

int recvAll(int s_descr, char *buf_ptr, size_t length, time_t *last_data_exchange_timestamp);

int setSocketOptions(int server_socket_dscr);

#endif //NOTER_SRV_NET_FUNC
