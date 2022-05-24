#include "app_config.hpp"

#include <syslog.h>

#include <string>
#include <map>
#include <fstream>
#include <sstream>

#include <noter_utils.hpp>

using namespace std;


void AppConfig::init() {
    if (initialized_.load()) {
        throw std::runtime_error("config is already initialized");
    }

    config_map_ = readConfigFile();

    initialized_.store(true);
}

const string AppConfig::getValue(string config_key) {
    if (!initialized_.load()) {
        throw std::runtime_error("config is not initialized");
    }

    if (!config_map_.count(config_key)) {
        return "";
    }
    //effectively thread safe for reads
    return config_map_.at(config_key);
}

map<string, string> AppConfig::readConfigFile() {
    map<string, string> config_map;

    ifstream config_file_stream = ifstream(CONFIG_FILE_PATH);
    
    if (!config_file_stream.is_open() || !config_file_stream.good()) {
        syslog(LOG_ERR, "failed to open app config file");
        
        return config_map;
    }

    string line;
    while (getline(config_file_stream, line)) {
        if (line.find('\r') != string::npos) {
            throw runtime_error("non-unix linebreaks in config file");
        }

        if (startsWith(line, "#")) {
            continue;
        }

        istringstream is_line(line);
        string key;

        if (getline(is_line, key, '=')) {
            string value;

            if (getline(is_line, value)) {
                config_map[key] = value;
            } 
        }
    }

    return config_map;
}
