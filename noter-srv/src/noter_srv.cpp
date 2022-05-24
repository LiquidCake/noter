#include "noter_srv.hpp"

#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <unistd.h>

#include <iostream>
#include <csignal>
#include <ctime>
#include <memory>
#include <fstream>
#include <thread>
#include <atomic>

#include "noter_utils.hpp"
#include "net_func.hpp"
#include "notes_consumer.hpp"
#include "notes_channels.hpp"
#include "app_config.hpp"

using namespace std;

#ifndef NDEBUG
    const bool DEBUG_ENABLED = true;
#else
    const bool DEBUG_ENABLED = false;
#endif

/* Constants */

extern const string OUT_FILES_TMP_DIR;
extern const string OUT_FILE_TMP_PREFIX;
extern const int SOCK_TIMEOUT_SEC;

const char* PORT = "8000";
/* nullptr is wildcard */
const char* HOSTNAME = nullptr;
const int SOCKET_QUEUE_LIMIT = 1024;

//3600 is 1hour
const int CLIENT_REQUEST_PROCESSING_TIMEOUT_SEC = 3600;

//32 bytes of uuid string + 4 dash separators
const int TEMP_FILE_NAME_BUFFER_LENGTH = 36;
//10485760 = 10 meg
const long TEMP_FILE_CONTENT_BUFFER_LENGTH = 10485760;
const int MD5_FILE_CONTENT_LENGTH = 32;

/* Variables */

atomic<bool> shutdown_requested;
static_assert(atomic<bool>::is_always_lock_free); //check atomic is lock free on this os


int main() {
    AppConfig::init();
    
    openlog("noter-srv", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
    if (!DEBUG_ENABLED) {
        setlogmask (LOG_UPTO(LOG_INFO));
    }

    syslog(LOG_INFO, "starting on port %s by user %d. Pid: %d", PORT, getuid(), getppid());

    //get address from OS. Will be linked list of addresses, we just use 1st
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_PASSIVE|AI_ADDRCONFIG;

    struct addrinfo *address = nullptr;
    int net_addr_reslt = getaddrinfo(HOSTNAME, PORT, &hints, &address);
    
    if (net_addr_reslt != 0) {
        syslog(LOG_ERR, "Failed getting net address, code %d. Message: %s", net_addr_reslt, strerror(errno));

        exit(EXIT_FAILURE);
    }

    //create server socket
    int server_sock_descr = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (server_sock_descr == -1) {
        syslog(LOG_ERR, "Error opening socket. Message: %s", strerror(errno));

        exit(EXIT_FAILURE);
    }
    
    //set socket options
    if (setSocketOptions(server_sock_descr) != Status::OK) {
        syslog(LOG_ERR, "Error at set_sock_options");

        exit(EXIT_FAILURE);
    }

    //bind socket to address
    if (bind(server_sock_descr, address->ai_addr, address->ai_addrlen) != 0) {
        syslog(LOG_ERR, "Error binding socket. Message: %s", strerror(errno));

        exit(EXIT_FAILURE);
    }

    //now we can release memory used for addrinfo
    freeaddrinfo(address);

    //listen
    if (listen(server_sock_descr, SOCKET_QUEUE_LIMIT) != 0) {
        syslog(LOG_ERR, "Error on calling listen(). Message: %s", strerror(errno));

        exit(EXIT_FAILURE);
    }

    //block sygnals to handle them later in separate thread
    sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigprocmask(SIG_BLOCK, &set, NULL);

    //make sure we dont wait any response from child processes
    signal(SIGCHLD, SIG_IGN);

    /* Start notes consumer thread */

    shutdown_requested.store(false);

    //create notes channels registry
    NotesChannelRegistry channels_registry;
    thread notes_consumer_thread(&watchTempFiles, ref(channels_registry));

    //start signal handling thread
    thread signal_handler_thread([&set, &notes_consumer_thread]() {
        while (true) {
            int sig;

            sig = sigwaitinfo(&set, nullptr);
            if (sig == -1) {
                if (errno == EINTR) {
                    //ignore signals we didn't wait for */
                    continue;
                }

                syslog(LOG_WARNING, "waiting for signal failed: %s", strerror(errno));

                continue;
            }

            syslog(LOG_INFO, "got termination signal, shutting down...");

            shutdown_requested.store(true);
            notes_consumer_thread.join();
            
            syslog(LOG_INFO, "exiting");

            exit(EXIT_SUCCESS);
        }
    });
    
    syslog(LOG_INFO, "Started OK");

    for (;;) {
        char client_ip[64];
        struct sockaddr_in client_addr;
        socklen_t socklen = sizeof(client_addr);
        
        int client_sock_descr = accept(server_sock_descr, reinterpret_cast<struct sockaddr*>(&client_addr), &socklen);
        
        if (client_sock_descr != -1) {
            strcpy(client_ip, inet_ntoa(client_addr.sin_addr));
        } else {
            //EAGAIN returns periodically due to SO_RCVTIMEO
            if (errno != EAGAIN && errno != EINTR) {
                syslog(LOG_ERR, "Failed to accept connection!. Message: %s", strerror(errno));
            }

            continue;
        }
        
        pid_t child_pid = fork();

        if (child_pid == 0) {
            //happens in child process
            
            close(server_sock_descr);

            processRequest(client_sock_descr);
            
            close(client_sock_descr);
            _exit(EXIT_SUCCESS);

        } else if (child_pid == -1) {
            syslog(LOG_ERR, "Failed to create child process. Message: %s", strerror(errno));
        }
        
        close(client_sock_descr);
                
        syslog(LOG_DEBUG, "[Got request from %s, passed to pid %d]", client_ip, child_pid);
    }
}

void processRequest(int sock_descr) {
    time_t processing_start_time_sec = time(0);
    HeapArrayContainer<char> file_content_buf(TEMP_FILE_CONTENT_BUFFER_LENGTH);

    //process all files client sends untill client closes socket. Global timeout just in case
    while (time(0) - processing_start_time_sec < CLIENT_REQUEST_PROCESSING_TIMEOUT_SEC) {
        char file_name[TEMP_FILE_NAME_BUFFER_LENGTH + 1] = {0};

        int bytes_read = recvAll(sock_descr, file_name, TEMP_FILE_NAME_BUFFER_LENGTH, nullptr);

        if (bytes_read == 0) {
            //client exited
            syslog(LOG_DEBUG, "got 0 bytes from client socket, assuming exit");

            return;
        }

        if (bytes_read != TEMP_FILE_NAME_BUFFER_LENGTH) {
            syslog(LOG_ERR, "failed to read file name: %s", strerror(errno));
            sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

            return;
        }

        uint32_t file_size_network_byteorder = 0;

        bytes_read = recvAll(
            sock_descr, 
            reinterpret_cast<char*>(&file_size_network_byteorder), 
            sizeof(file_size_network_byteorder), nullptr
        );
        if (bytes_read != sizeof(file_size_network_byteorder)) {
            syslog(LOG_ERR, "failed to read file size '%s': %s", file_name, strerror(errno));
            sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

            return;
        }

        size_t file_size = ntohl(file_size_network_byteorder);

        if (file_size <= 0) {
            syslog(LOG_ERR, "got invalid file size for '%s': %li", file_name, file_size);
            sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

            return;
        }

        char md5_file_content[MD5_FILE_CONTENT_LENGTH + 1] = {0};

        bytes_read = recvAll(sock_descr, md5_file_content, MD5_FILE_CONTENT_LENGTH, nullptr);
        if (bytes_read != MD5_FILE_CONTENT_LENGTH) {
            syslog(LOG_ERR, "failed to read md5 file '%s': %s", file_name, strerror(errno));
            sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

            return;
        }

        /* Receive / save file contents */

        syslog(LOG_DEBUG, "receiving file '%s' of size '%li'", file_name, file_size);

        string out_file_path_tmp = OUT_FILES_TMP_DIR + OUT_FILE_TMP_PREFIX + string(file_name);

        ofstream out_file_stream = ofstream(out_file_path_tmp, ios::out | ios::binary);

        if (!out_file_stream.is_open() || !out_file_stream.good()) {
            syslog(LOG_ERR, "failed to open temp output file '%s': %s", out_file_path_tmp.c_str(), strerror(errno));
            sendProcessedResponse(sock_descr, ProcessingStatus::SERVER_INTERNAL_ERROR);

            return;
        }

        long bytes_to_receive = file_size;

        while (bytes_to_receive > 0) {
            int bytes_chunk = min(TEMP_FILE_CONTENT_BUFFER_LENGTH, bytes_to_receive);

            //receive file chunk
            if (recvAll(sock_descr, file_content_buf.data(), bytes_chunk, nullptr) <= 0) {
                syslog(LOG_ERR, "error while recieving file chunk for '%s': '%s'", file_name, strerror(errno));
                out_file_stream.close();
                deleteFile(out_file_path_tmp);
                sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

                return;
            }

            //read file chunk
            out_file_stream.write(file_content_buf.data(), bytes_chunk);

            if (!out_file_stream.good()) {
                syslog(LOG_ERR, "error while writing tmp file '%s': '%s'", out_file_path_tmp.c_str(), strerror(errno));
                out_file_stream.close();
                deleteFile(out_file_path_tmp);
                sendProcessedResponse(sock_descr, ProcessingStatus::SERVER_INTERNAL_ERROR);

                return;
            }

            bytes_to_receive -= bytes_chunk;
        }

        out_file_stream.close();

        if (!out_file_stream.good()) {
            syslog(LOG_ERR, "failed to close temp output file '%s': %s", out_file_path_tmp.c_str(), strerror(errno));
            deleteFile(out_file_path_tmp);
            sendProcessedResponse(sock_descr, ProcessingStatus::SERVER_INTERNAL_ERROR);

            return;
        }

        string file_md5_str;
        if (calculateFileMD5(out_file_path_tmp, &file_md5_str) != Status::OK) {
            syslog(LOG_ERR, "failed to calculate file md5: '%s'", out_file_path_tmp.c_str());
            deleteFile(out_file_path_tmp);
            sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

            return;
        }

        if (file_md5_str != string(md5_file_content)) {
            syslog(LOG_ERR, "error: temp file md5 doesnt match: '%s'", out_file_path_tmp.c_str());
            deleteFile(out_file_path_tmp);
            sendProcessedResponse(sock_descr, ProcessingStatus::DATA_TRANSFER_ERROR);

            return;
        }

        string out_file_path_final = OUT_FILES_TMP_DIR + string(file_name);
        if (renameFile(out_file_path_tmp, out_file_path_final) != Status::OK) {
            syslog(LOG_ERR, "failed to rename temp file to final name: '%s'", out_file_path_tmp.c_str());
            deleteFile(out_file_path_tmp);
            sendProcessedResponse(sock_descr, ProcessingStatus::SERVER_INTERNAL_ERROR);

            return;
        }

        sendProcessedResponse(sock_descr, ProcessingStatus::OK);
        syslog(LOG_INFO, "successfully received file %s", out_file_path_final.c_str());
    }
}

int sendProcessedResponse(int s_descr, ProcessingStatus status) {
    int status_code = static_cast<int>(status);
    return sendAll(s_descr, reinterpret_cast<char*>(&status_code), sizeof(status_code));
}
