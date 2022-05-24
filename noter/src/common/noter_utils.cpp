#include "noter_utils.hpp"

#include <openssl/md5.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <syslog.h>

using namespace std;

extern const string OUT_FILES_TMP_DIR = "/tmp/noter/";
extern const string OUT_FILE_TMP_PREFIX = "temp_";

//1048576000 = 1000 mb
extern const size_t MAX_OUT_FILE_SIZE = 1048576000L;

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
            cout << "failed to delete file " << file_path << endl;
            syslog(LOG_ERR, "failed to delete file %s", file_path.c_str());
        }
    } catch (const filesystem::filesystem_error& err) {
        cout << "error during attempt to delete file " << err.what() << endl;
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
        cout << "error during attempt to rename file " << err.what() << endl;
        syslog(LOG_ERR, "error during attempt to rename file %s: %s", old_path.c_str(), err.what());
    }
    
    return Status::ERROR;
}

int createDirectories(const string full_path) {
    try {
        filesystem::create_directories(full_path);

        return Status::OK;
    } catch (const filesystem::filesystem_error& err) {
        cout << "error during attempt to create directories " << err.what() << endl;
        syslog(LOG_ERR, "error during attempt to create directories %s: %s", full_path.c_str(), err.what());
    }
    
    return Status::ERROR;
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
