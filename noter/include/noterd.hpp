#ifndef NOTER_NOTERD
#define NOTER_NOTERD

#include <sys/types.h>

enum class ProcessingStatus {
    OK = 100,
    GENERIC_ERROR = 101,
    DATA_TRANSFER_ERROR = 102,
    SERVER_INTERNAL_ERROR = 103
};

int initDaemon(pid_t pid);

void doHeartbeat();

void connectSocketLoop();

int connectSocket();

void closeSocket();

void registerSignalHandlers();

void sigHandler(int sig_num);

#endif //NOTER_NOTERD
