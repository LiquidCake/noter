#include "net_func.hpp"

#include <syslog.h>
#include <errno.h>
#include <fcntl.h>

#include <ctime>
#include <cstring>

#include "noter_utils.hpp"

using namespace std;

extern const int SOCK_TIMEOUT_SEC = 60;


int sendAll(int s_descr, const char* buff, size_t length) {
    time_t send_start_time_sec = time(0);
    time_t time_elapsed;

    while (length > 0) {
        int res = send(s_descr, buff, length, 0);

        //make sure request times out even if data is sent but too slow
        time_t curr_time_sec = time(0);
        time_elapsed = curr_time_sec - send_start_time_sec;
        if (time_elapsed > SOCK_TIMEOUT_SEC) {
            errno = ETIMEDOUT;

            return Status::ERROR;
        }
            
        if (res == -1) {
            //continue if interrupted
            if (errno == EINTR) {
                continue;
            } else {

                return Status::ERROR;
            }
        }

        length -= res;
    }

    return Status::OK;
}

int recvAll(int s_descr, char *buf_ptr, size_t length, time_t *last_data_exchange_timestamp) {
    time_t recv_start_time_sec = time(0);
    time_t time_elapsed;
    
    size_t bytes_read = 0;
    
    while (length > 0) {
        int res = recv(s_descr, buf_ptr + bytes_read, length, 0);

        //make sure request times out even if data comes but too slow
        time_t curr_time_sec = time(0);
        time_elapsed = curr_time_sec - recv_start_time_sec;
        if (time_elapsed > SOCK_TIMEOUT_SEC) {
            errno = ETIMEDOUT;

            return -1;
        }
            
        if (res == -1) {
            //continue if interrupted
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }

        //some data received - remember current time
        if (last_data_exchange_timestamp != nullptr) {
            *last_data_exchange_timestamp = time(0);
        }

        if (res == 0) {
            break;
        }

        bytes_read += res;
        length -= res;
    }
    
    return bytes_read;
}

int setSocketOptions(int server_socket_dscr) {
    //set reuse address
    int reuseaddr = 1;
    if (setsockopt(server_socket_dscr, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseaddr), sizeof(reuseaddr)) != 0) {
        syslog(LOG_ERR, "Error setting socket option SO_REUSEADDR. Message: %s", strerror(errno));
        
        return Status::ERROR;
    }
    
    //set read timeouts
    struct timeval timeout;      
    timeout.tv_sec = SOCK_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    if (setsockopt(server_socket_dscr, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout)) != 0) {
        syslog(LOG_ERR, "Error setting socket option SO_RCVTIMEO. Message: %s", strerror(errno));
        
        return Status::ERROR;
    }

    if (setsockopt(server_socket_dscr, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout)) != 0) {
        syslog(LOG_ERR, "Error setting socket option SO_SNDTIMEO. Message: %s", strerror(errno));
        
        return Status::ERROR;
    }
        
    return Status::OK;
}
