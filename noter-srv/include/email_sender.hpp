#ifndef NOTER_EMAIL_SENDER
#define NOTER_EMAIL_SENDER

#include "sole.hpp"

#include <mutex>

class EmailSender {
public:
    EmailSender() = delete;
    ~EmailSender() = delete;

    EmailSender(const EmailSender& other) = delete;
    EmailSender& operator= (const EmailSender& other) = delete;

    static int sendEmail(std::string subject, std::string payload);

    static std::string getPayload() { return payload_; };
    static void setPayload(std::string payload) { payload_ = payload; };
        
private:
    static std::string formatCurrentDateForEmail();

    static void initPayload(
        std::string subject, 
        std::string from_mail, 
        std::string to_mail,
        std::string message_id_domain, 
        std::string payload
    );

    static void clearPayload() { payload_ = ""; };

    static size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp);

    inline static std::string payload_;
    inline static std::mutex email_sender_mutex_;
};


#endif //NOTER_EMAIL_SENDER