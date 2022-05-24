#include "noter_utils.hpp"

#include <openssl/md5.h>
#include <sys/stat.h>
#include <syslog.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>

using namespace std;

extern const string OUT_FILES_TMP_DIR = "/tmp/noter_srv/";
extern const string OUT_FILE_TRANSFER_DIR = OUT_FILES_TMP_DIR + "/transfer/";
extern const string OUT_FILE_ARCHIVED_DIR = OUT_FILES_TMP_DIR + "/archive/";
extern const string OUT_FILE_TMP_PREFIX = "temp_";
extern const string OUT_FILE_ARCHIVED_PREFIX = "noter_arch_";

extern const string META_KEY_TIMESTAMP = "ts";
extern const string META_KEY_OS = "os";
extern const string META_KEY_CHANNEL = "ch";

const int MD5_CALCULATION_FILE_READ_BUFF_SIZE = 1024 * 1000;

bool fileExists(const string file_path) {
    return filesystem::exists(file_path);
}

int deleteFile(const string file_path) {
    if (!fileExists(file_path)) {
        return Status::OK;
    }

    try {
        //on POSIX expected to call unlink() so should delete even after stream.close() failure
        if (filesystem::remove(file_path)) {
            return Status::OK;
        } else {
            syslog(LOG_ERR, "failed to delete file %s", file_path.c_str());
        }
    } catch (const filesystem::filesystem_error& err) {
        syslog(LOG_ERR, "error during attempt to delete file %s: %s", file_path.c_str(), err.what());
    }
    
    return Status::ERROR;
}

int renameFile(const string old_path, const string new_path) {
    if (fileExists(new_path)) {
        if (deleteFile(new_path) != 0) {
            return Status::ERROR;
        }
    }

    try {
        filesystem::rename(old_path, new_path);

        return Status::OK;
    } catch (const filesystem::filesystem_error& err) {
        syslog(LOG_ERR, "error during attempt to rename file %s: %s", old_path.c_str(), err.what());
    }
    
    return Status::ERROR;
}

int createDirectories(const string full_path) {
    try {
        filesystem::create_directories(full_path);

        return Status::OK;
    } catch (const filesystem::filesystem_error& err) {
        syslog(LOG_ERR, "error during attempt to create directories %s: %s", full_path.c_str(), err.what());
    }
    
    return Status::ERROR;
}

int copyFile(const string orig_path, const string dest_path) {
    try {
        filesystem::copy(orig_path, dest_path);

        return Status::OK;
    } catch (const filesystem::filesystem_error& err) {
        syslog(LOG_ERR, "error during attempt to copy file %s: %s", orig_path.c_str(), err.what());
    }
    
    return Status::ERROR;
}

int createOrClearDirectory(string dir_path) {
    if (createDirectories(dir_path) != Status::OK) {
        syslog(LOG_ERR, "failed to create transfer directory");

        return Status::ERROR;
    }

    for (const auto& entry : filesystem::directory_iterator(dir_path)) {
        filesystem::path entry_path = entry.path();
        string file_path = entry_path.string();

        if (filesystem::is_directory(entry_path)) {
            continue;
        }

        if (deleteFile(file_path) != Status::OK) {
            syslog(LOG_ERR, "failed to delete file %s", file_path.c_str());

            return Status::ERROR;
        }
    }

    return Status::OK;
}

long getFileSize(string file_path) {
    struct stat file_stats;

    if (stat(file_path.c_str(), &file_stats) != 0) {
        return -1;
    }

    return static_cast<long>(file_stats.st_size);
}

int calculateFileMD5(const string file_path, std::string *out_str) {
    ifstream file(file_path, ifstream::binary);
    if (!file.is_open() || !file.good()) {
        return Status::ERROR;
    }

    MD5_CTX md5_context;
    if (MD5_Init(&md5_context) != 1) {
        return Status::ERROR;
    }
    
    long file_size;
    if ((file_size = getFileSize(file_path)) == -1) {
        return Status::ERROR;
    }

    char buf[MD5_CALCULATION_FILE_READ_BUFF_SIZE];

    while (file.good()) {
        file.read(buf, sizeof(buf));

        int byte_read = file.gcount();
        MD5_Update(&md5_context, buf, byte_read);

        file_size -= byte_read;
    }

    if (file_size != 0) {
        return Status::ERROR;
    }
    
    unsigned char result_as_numbers[MD5_DIGEST_LENGTH];
    if (MD5_Final(result_as_numbers, &md5_context) != 1) {
        return Status::ERROR;
    }

    //convert to hex nums string
    stringstream md5_string;
    md5_string << hex << uppercase << setfill('0');

    for (const unsigned char& byte : result_as_numbers) {
        md5_string << setw(2) << static_cast<int>(byte);
    }

    *out_str = md5_string.str();

    return Status::OK;
}

bool startsWith(string str, string pref) {
    if (str.size() < pref.size()) {
        return false;
    }

    return str.substr(0, pref.size()) == pref;
}

bool endsWith(string str, string ending) {
    if (str.size() < ending.size()) {
        return false;
    }

    return str.substr(str.size() - ending.size(), str.size()) == ending;
}

vector<string> split_string_by_delim(string str, string delim) {
    size_t pos = 0;
    string token;
    vector<string> result;

    while ((pos = str.find(delim)) != string::npos) {
        token = str.substr(0, pos);

        result.push_back(token);

        str.erase(0, pos + delim.length());
    }

    if (str.size() > 0) {
        result.push_back(str);
    }

    return result;
}

string timestampToString(long timestamp) {
    time_t temp = timestamp;
    tm* t = std::gmtime(&temp);
    stringstream ss;
    ss << put_time(t, "%Y-%m-%d %I:%M:%S %p %z");
    return ss.str();
}

string stringMapToJson(map<string, string>& string_map) {
    stringstream ss;
    ss << "{";

    for (map<string, string>::iterator it = string_map.begin(); it != string_map.end(); it++) {
        ss << "\"" << it->first << "\": " << "\"" << it->second << "\",";
    }

    string str = ss.str();

    if (endsWith(str, ",")) {
        str = str.substr(0, str.size() - 1);
    }

    str += "}";
    return str;
}
