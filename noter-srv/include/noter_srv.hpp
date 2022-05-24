#ifndef NOTER_SRV
#define NOTER_SRV

enum class ProcessingStatus {
    OK = 100,
    GENERIC_ERROR = 101,
    DATA_TRANSFER_ERROR = 102,
    SERVER_INTERNAL_ERROR = 103
};

void processRequest(int sock_descr);

int sendProcessedResponse(int s_descr, ProcessingStatus status);

#endif //NOTER_SRV
