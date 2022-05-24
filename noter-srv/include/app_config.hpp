#ifndef NOTER_APP_CONFIG
#define NOTER_APP_CONFIG

#include <string>
#include <map>
#include <atomic>

const std::string CONFIG_FILE_PATH = "/etc/noter-srv/config.cfg";

const std::string CONFIG_EMAIL_MESSAGE_ID_DOMAIN = "email_message_id_domain";
const std::string CONFIG_EMAIL_SMTP_URL = "email_smtp_url";
const std::string CONFIG_EMAIL_ACCOUNT_USERNAME = "email_account_username";
const std::string CONFIG_EMAIL_ACCOUNT_PASSWORD = "email_account_password";
const std::string CONFIG_EMAIL_TO_MAIL_ADDR = "email_to_mail_addr";
const std::string CONFIG_EMAIL_FROM_MAIL_ADDR = "email_from_mail_addr";

const std::string CONFIG_DB_URL = "db_url";
const std::string CONFIG_DB_DATABASE = "db_database";
const std::string CONFIG_DB_USERNAME = "db_username";
const std::string CONFIG_DB_PASSWORD = "db_password";

const std::string CONFIG_DELETE_NOTE_AFTER_PROCESSING = "delete_note_after_processing";


class AppConfig {
public:
    AppConfig() = delete;
    ~AppConfig() = delete;

    static void init();

    static const std::string getValue(std::string config_key);

private:
    static std::map<std::string, std::string> readConfigFile();

    inline static std::atomic<bool> initialized_;
    inline static std::map<std::string, std::string> config_map_;
};

#endif //NOTER_APP_CONFIG
