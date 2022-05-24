#ifndef NOTER_APP_CONFIG
#define NOTER_APP_CONFIG

#include <string>
#include <map>
#include <atomic>

const std::string CONFIG_FILE_PATH = "/etc/noter/config.cfg";

const std::string CONFIG_KEY_CHANNEL = "send_channel";
const std::string CONFIG_NOTER_SRV_ADDR = "noter_srv_addr";

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
