#include "noterd.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#include <csignal>
#include <ctime>
#include <fstream>
#include <string>
#include <cstring>
#include <filesystem>

#include <noter_utils.hpp>
#include <net_func.hpp>
#include <app_config.hpp>

using namespace std;

#ifndef NDEBUG
    const bool DEBUG_ENABLED = true;
#else
    const bool DEBUG_ENABLED = false;
#endif

/* Constants */

extern const string OUT_FILES_TMP_DIR;
extern const string OUT_FILE_TMP_PREFIX;

extern const long unsigned int MAX_OUT_FILE_SIZE;

extern const int SOCK_TIMEOUT_SEC;

const int SLEEP_INTERVAL_SEC = 5;
const int SOCKET_RECONNECT_INTERVAL_SEC = 10;
const long int MAX_TMP_IDLE_TIME_SEC = 86400L;

//10485760 = 10 meg
const long int FILE_CONTENT_BUFFER_LENGTH = 10485760;

const string PID_FILE_PATH = "/run/noterd.pid";
//32 bytes of uuid string + 4 dash separators
const int TEMP_FILE_NAME_LENGTH = 36;
const int MD5_FILE_CONTENT_LENGTH = 32;

const int SRV_PORT = 8000;

/* Variables */

struct sockaddr_in srv_addr;

HeapArrayContainer<char> file_content_buf(FILE_CONTENT_BUFFER_LENGTH);

int sock_descr = -1;


int main() {
    pid_t pid = fork();

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    umask(0);

    openlog("daemon-noterd", LOG_PID | LOG_NDELAY, LOG_USER);
    if (!DEBUG_ENABLED) {
        setlogmask (LOG_UPTO(LOG_INFO));
    }
    
    syslog(LOG_INFO, "started daemon");

    pid_t sesssion_id = setsid();

    if (sesssion_id < 0) {
        syslog(LOG_ERR, "error could not generate session ID for child process");

        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "error could not change working directory to /");

        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    registerSignalHandlers();

    if (initDaemon(sesssion_id) != Status::OK) {
        syslog(LOG_ERR, "error on daemon init");

        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "starting heartbeat loop");

    while (1) {
        doHeartbeat();

        sleep(SLEEP_INTERVAL_SEC);
    }

    closelog();

    exit(EXIT_SUCCESS);
}

int initDaemon(pid_t pid) {
    AppConfig::init();

    if (!fileExists(OUT_FILES_TMP_DIR)) {
        if (createDirectories(OUT_FILES_TMP_DIR) != 0) {
            syslog(LOG_ERR, "failed to create output file directory");

            return Status::ERROR;
        }
    }

    if (deleteFile(PID_FILE_PATH) != 0) {
        syslog(LOG_ERR, "error while deleting old pid file");

        return Status::ERROR;
    }

    ofstream pid_file_ostream = ofstream(PID_FILE_PATH, ios::out | ios::binary);

    if (!pid_file_ostream.is_open() || !pid_file_ostream.good()) {
        syslog(LOG_ERR, "error while opening pid file: '%s'", strerror(errno));

        return Status::ERROR;
    }

    string pid_str = to_string(static_cast<int>(pid)) + '\n';

    pid_file_ostream.write(pid_str.c_str(), pid_str.size());

    if (!pid_file_ostream.good()) {
        syslog(LOG_ERR, "error while writing to pid file: '%s'", strerror(errno));

        return Status::ERROR;
    }

    pid_file_ostream.close();

    if (!pid_file_ostream.good()) {
        syslog(LOG_ERR, "error while closing pid file: '%s'", strerror(errno));

        return Status::ERROR;
    }

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SRV_PORT);

    //convert IP from text to binary
    string srv_host = AppConfig::getValue(CONFIG_NOTER_SRV_ADDR);
    if (inet_pton(AF_INET, srv_host.c_str(), &srv_addr.sin_addr) <= 0) {
        syslog(LOG_ERR, "convert IP address '%s' from text to bin: '%s'", srv_host.c_str(), strerror(errno));

        return Status::ERROR;
    }

    return Status::OK;
}

void doHeartbeat() {
    syslog(LOG_DEBUG, "hearthbeat");

    //iterate files in dir, remove possible old tmp files and process new ones
    time_t curr_time_sec = time(0);
    struct stat file_stats;

    for (const auto& entry : filesystem::directory_iterator(OUT_FILES_TMP_DIR)) {
        filesystem::path entry_path = entry.path();
        string file_name = entry_path.filename().string();
        string file_path = entry_path.string();

        if (filesystem::is_directory(entry_path) || entry_path.extension().string() == ".md5") {
            continue;
        }

        if (stat(file_path.c_str(), &file_stats) != 0) {
            syslog(LOG_ERR, "error while reading temp file '%s': '%s'", file_path.c_str(), strerror(errno));

            continue;
        }

        if (startsWith(file_name, OUT_FILE_TMP_PREFIX) && curr_time_sec - file_stats.st_ctime > MAX_TMP_IDLE_TIME_SEC) {
            deleteFile(file_path);
            deleteFile(file_path + ".md5");

            syslog(LOG_INFO, "deleted dangling temp file '%s'", file_path.c_str());

            continue;
        }

        /* Process tmp file */

        syslog(LOG_INFO, "processing temp file '%s'", file_path.c_str());

        if (file_name.size() != TEMP_FILE_NAME_LENGTH) {
            syslog(LOG_WARNING, "found temp file with bad name: '%s'", file_path.c_str());

            continue;
        }

        size_t file_size = static_cast<size_t>(file_stats.st_size);

        if (file_size == 0 || file_size > MAX_OUT_FILE_SIZE) {
            syslog(LOG_WARNING, "found temp file with invalid size - %li max is %lui: '%s'", 
                file_size, MAX_OUT_FILE_SIZE, file_path.c_str());

            continue;
        }

        //send file info to noter server

        //(re)connect to noter server
        if (sock_descr == -1) {
            connectSocketLoop();
        }

        //send file name
        if (sendAll(sock_descr, file_name.c_str(), file_name.size()) != Status::OK) {
            syslog(LOG_ERR, "failed to send file name: '%s'", strerror(errno));

            break;
        }

        //send file size
        uint32_t file_size_network_byteroder = htonl(file_size);
        if (sendAll(sock_descr, reinterpret_cast<char*>(&file_size_network_byteroder), sizeof(file_size_network_byteroder))
             != Status::OK) {
            syslog(LOG_ERR, "failed to send file size: '%s'", strerror(errno));

            break;
        }

        //send md5 file contents
        ifstream md5_f_stream(file_path + ".md5", ios::out | ios::binary);

        if (!md5_f_stream.is_open() || !md5_f_stream.good()) {
            syslog(LOG_ERR, "failed to open md5 file '%s': '%s'", file_path.c_str(), strerror(errno));

            continue;
        }

        char md5_file_buf[MD5_FILE_CONTENT_LENGTH];

        md5_f_stream.read(md5_file_buf, MD5_FILE_CONTENT_LENGTH);

        if (!md5_f_stream.good()) {
            syslog(LOG_ERR, "failed to read md5 file for '%s': '%s'", file_path.c_str(), strerror(errno));

            continue;
        }

        if (sendAll(sock_descr, md5_file_buf, MD5_FILE_CONTENT_LENGTH) != Status::OK) {
            syslog(LOG_ERR, "failed to send md5 file for '%s': '%s'", file_path.c_str(), strerror(errno));

            break;
        }

        //send file content

        ifstream f_stream(file_path, ios::out | ios::binary);

        if (!f_stream.is_open() || !f_stream.good()) {
            syslog(LOG_ERR, "failed to open file '%s': '%s'", file_path.c_str(), strerror(errno));

            continue;
        }

        long bytes_to_send = file_size;
        bool file_error = false;
        bool sock_error = false;

        while (bytes_to_send > 0 && !file_error && !sock_error) {
            int bytes_chunk = min(FILE_CONTENT_BUFFER_LENGTH, bytes_to_send);

            //read file chunk
            f_stream.read(file_content_buf.data(), bytes_chunk);

            if (!f_stream.good()) {
                file_error = true;
                break;
            }

            //send file chunk
            if (sendAll(sock_descr, file_content_buf.data(), bytes_chunk) != Status::OK) {
                sock_error = true;
                break;
            }

            bytes_to_send -= bytes_chunk;
        }

        f_stream.close();

        if (file_error) {
            syslog(LOG_ERR, "error while reading file '%s': '%s'", file_path.c_str(), strerror(errno));

            continue;
        }

        if (sock_error) {
            syslog(LOG_ERR, "error while sending file chunk '%s': '%s'", file_path.c_str(), strerror(errno));

            break;
        }

        syslog(LOG_INFO, "sent temp file '%s' of length '%li'", file_path.c_str(), file_size);

        int resp_code = -1;
        int status_bytes_read = recvAll(sock_descr, reinterpret_cast<char*>(&resp_code), sizeof(resp_code), nullptr);

        if (status_bytes_read <= 0) {
            syslog(LOG_ERR, "error while reading request status for file '%s': '%s'", file_path.c_str(), strerror(errno));

            break;
        }

        if (resp_code == static_cast<int>(ProcessingStatus::OK)) {
            syslog(LOG_INFO, "successfully processed/sent file '%s' of length '%li'", file_path.c_str(), file_size);

            deleteFile(file_path);
            deleteFile(file_path + ".md5");
        } else {
            syslog(LOG_ERR, "failed to send temp file '%s' of length '%li'. Response status: '%i'", 
                file_path.c_str(), file_size, resp_code);
        }
    }

    //after attempt to send files - close socket until next heartbeat
    closeSocket();
}

void connectSocketLoop() {
    syslog(LOG_DEBUG, "opening socket to server");

    while (connectSocket() == Status::ERROR) {
        closeSocket();
        sleep(SOCKET_RECONNECT_INTERVAL_SEC);
    }

    syslog(LOG_INFO, "connected to server");
}

int connectSocket() {
    if ((sock_descr = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "failed to create socket to server: '%s'", strerror(errno));

        return Status::ERROR;
    }

    if (setSocketOptions(sock_descr) != Status::OK) {
        syslog(LOG_ERR, "Error at set_sock_options");

        return Status::ERROR;
    }

    struct timeval sock_timeout;
    sock_timeout.tv_sec = SOCK_TIMEOUT_SEC;
    sock_timeout.tv_usec = 0;

    if (connectWait(sock_descr, reinterpret_cast<struct sockaddr*>(&srv_addr), sizeof(srv_addr), &sock_timeout) != Status::OK) {
        syslog(LOG_ERR, "failed to connect socket to server: '%s'", strerror(errno));

        return Status::ERROR;
    }

    return Status::OK;
}

void closeSocket() {
    if (sock_descr != -1) {
        shutdown(sock_descr, SHUT_RDWR);
    }

    sock_descr = -1;
}

void registerSignalHandlers() {
    signal(SIGINT, sigHandler);
    signal(SIGABRT, sigHandler);
    signal(SIGTERM, sigHandler);

    //prevent SIGPIPE on closed socket
    signal(SIGPIPE, SIG_IGN);
}

void sigHandler(int sig_num) {
    exit(sig_num);
}
