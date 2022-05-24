#include <email_sender.hpp>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <curl/curl.h>

#include <string>
#include <mutex>

#include "sole.hpp"
#include "noter_utils.hpp"
#include "app_config.hpp"

using namespace std;

struct upload_status {
    size_t bytes_read;
};

int EmailSender::sendEmail(string subject, string payload) {
    lock_guard<mutex> guard(email_sender_mutex_);

    CURLcode res = CURLE_FAILED_INIT;
    CURL * curl = curl_easy_init();

    if (curl) {
        const string message_id_domain = AppConfig::getValue(CONFIG_EMAIL_MESSAGE_ID_DOMAIN);
        const string smtp_url = AppConfig::getValue(CONFIG_EMAIL_SMTP_URL);
        const string email_account_username = AppConfig::getValue(CONFIG_EMAIL_ACCOUNT_USERNAME);
        const string email_account_password = AppConfig::getValue(CONFIG_EMAIL_ACCOUNT_PASSWORD);

        string to_mail_addr = AppConfig::getValue(CONFIG_EMAIL_TO_MAIL_ADDR);
        string from_mail_addr = AppConfig::getValue(CONFIG_EMAIL_FROM_MAIL_ADDR);

        if (message_id_domain == "" 
            || smtp_url == ""
            || email_account_username == ""
            || email_account_password == ""
            || to_mail_addr == ""
            || from_mail_addr == ""
        ) {
            syslog(LOG_ERR, "bad email configs");
            return -1;
        }

        to_mail_addr = "<" + to_mail_addr + ">";
        from_mail_addr = "<" + from_mail_addr + ">";

        initPayload(subject, from_mail_addr, to_mail_addr, message_id_domain, payload);

        curl_easy_setopt(curl, CURLOPT_URL, smtp_url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, email_account_username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, email_account_password.c_str());

        curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from_mail_addr.c_str());

        struct curl_slist *recipients = nullptr;
        recipients = curl_slist_append(recipients, to_mail_addr.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        // callback function that provides payload
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, EmailSender::payload_source);
        
        struct upload_status upload_ctx = { 0 };
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

        //send the message
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            syslog(LOG_ERR, "error while sending email: %s", curl_easy_strerror(res));
        }

        curl_slist_free_all(recipients);

        curl_easy_cleanup(curl);

        clearPayload();
    }

    return static_cast<int>(res);
}

string EmailSender::formatCurrentDateForEmail() {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(buffer, 80, "%a, %d %b %Y %T %z", timeinfo);

    return string(buffer);
}

void EmailSender::initPayload(
    string subject, 
    string from_mail, 
    string to_mail,
    string message_id_domain, 
    string payload
) {
    setPayload(
        "Date: " + formatCurrentDateForEmail() + "\r\n" +
        "To: " + to_mail + "\r\n" +
        "From: " + from_mail + "\r\n" +
        "Message-ID: <" + sole::uuid1().str() + "@" + message_id_domain + ">\r\n" +
        "Subject: " + subject + "\r\n" +
        "\r\n" + payload + "\r\n"
    );
}

size_t EmailSender::payload_source(char *ptr, size_t size, size_t nmemb, void *userp) {
    struct upload_status *upload_ctx = reinterpret_cast<struct upload_status*>(userp);
    const char *data;
    size_t room = size * nmemb;

    if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1)) {
        return 0;
    }

    string payload = getPayload();
    const char *payload_arr = payload.c_str();

    data = &payload_arr[upload_ctx->bytes_read];

    if (data) {
        size_t len = strlen(data);

        if (room < len) {
            len = room;
        }

        memcpy(ptr, data, len);
        upload_ctx->bytes_read += len;

        return len;
    }

    return 0;
}
