#include "net_func.hpp"

#include <syslog.h>
#include <errno.h>
#include <fcntl.h>

#include <ctime>
#include <cstring>

#include <noter_utils.hpp>

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

int connectWait(int sock_descr, struct sockaddr *addr, size_t addrlen, struct timeval *timeout) {
    int socket_ready_res, sock_options_flags;

    //get socket flags
    if ((sock_options_flags = fcntl(sock_descr, F_GETFL, nullptr)) < 0) {
        return Status::ERROR;
    }

    //set socket non-blocking
    if (fcntl(sock_descr, F_SETFL, sock_options_flags | O_NONBLOCK) < 0) {
        return Status::ERROR;
    }

    int num_sockets_ready_after_select = -1;

    //try to connect
    if ((socket_ready_res = connect(sock_descr, addr, addrlen)) != 0) {
        if (errno == EINPROGRESS) {
            fd_set to_be_written_descr_set;

            //make empty file descr set, add socket to set
            FD_ZERO(&to_be_written_descr_set);
            FD_SET(sock_descr, &to_be_written_descr_set);

            //pass file descr set to select and wait for socket to be writable
            //returns number of ready sockets once any available or after given timeout
            num_sockets_ready_after_select = select(sock_descr + 1, nullptr, &to_be_written_descr_set, nullptr, timeout);
        }
    }

    //reset socket flags to initial state (blocking)
    if (fcntl(sock_descr, F_SETFL, sock_options_flags) < 0) {
        return Status::ERROR;
    }

    //if socket opened (immediately or after waiting)
    if (socket_ready_res == 0 || num_sockets_ready_after_select > 0) {
        socklen_t len = sizeof(sock_options_flags);

        //check for error flags in socket (reuse flags var)
        if (getsockopt(sock_descr, SOL_SOCKET, SO_ERROR, &sock_options_flags, &len) < 0) {
            return Status::ERROR;
        }

        // there was an error
        if (sock_options_flags) {
            errno = sock_options_flags;
            return Status::ERROR;
        }

        return Status::OK;
    }

    if (num_sockets_ready_after_select == 0) {
        //select timed out
        errno = ETIMEDOUT;
    }

    //else - an error occured in either connect or select

    return Status::ERROR;
}

int setSocketOptions(int server_socket_dscr) {
    struct timeval sock_timeout;
    sock_timeout.tv_sec = SOCK_TIMEOUT_SEC;
    sock_timeout.tv_usec = 0;

    if (setsockopt(server_socket_dscr, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&sock_timeout), sizeof(sock_timeout)) != 0) {
        syslog(LOG_ERR, "Error setting socket option SO_RCVTIMEO. Message: %s", strerror(errno));
        
        return Status::ERROR;
    }

    if (setsockopt(server_socket_dscr, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&sock_timeout), sizeof(sock_timeout)) != 0) {
        syslog(LOG_ERR, "Error setting socket option SO_SNDTIMEO. Message: %s", strerror(errno));
        
        return Status::ERROR;
    }
        
    return Status::OK;
}
