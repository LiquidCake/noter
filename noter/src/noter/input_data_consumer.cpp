#include "input_data_consumer.hpp"

#include <stdlib.h>

#include <ctime>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <array>
#include <vector>

#include "sole.hpp"

#include "noter_utils.hpp"
#include "app_config.hpp"

#ifndef NDEBUG
    const bool DEBUG_ENABLED = true;
#else
    const bool DEBUG_ENABLED = false;
#endif

using namespace std;

extern const string OUT_FILES_TMP_DIR;
extern const size_t MAX_OUT_FILE_SIZE;
extern const string OUT_FILE_TMP_PREFIX;

//10485760 = 10 meg
const int FILE_READ_BUFFER_LENGTH = 10485760;

const string META_KEY_TIMESTAMP = "ts";
const string META_KEY_OS = "os";
const string META_KEY_CHANNEL = "ch";


int InputDataConsumer::readAndTransferData() {
    //reopen stdin in binary mode
    freopen(nullptr, "rb", stdin);
    
    if (ferror(stdin)) {
        cout << "error while re-opening stdin: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }

    if (!fileExists(OUT_FILES_TMP_DIR)) {
        if (createDirectories(OUT_FILES_TMP_DIR) != 0) {
            cout << "failed to create output file directory" << endl;
        
            return Status::ERROR;
        }
    }

    //open output file
    
    out_file_uuid_ = sole::uuid1().str();
    out_file_path_final_ = OUT_FILES_TMP_DIR + out_file_uuid_;
    out_file_path_tmp_ = OUT_FILES_TMP_DIR + OUT_FILE_TMP_PREFIX + out_file_uuid_;

    out_file_stream_ = ofstream(out_file_path_tmp_, ios::out | ios::binary);

    if (!out_file_stream_.is_open() || !out_file_stream_.good()) {
        cout << "error while opening output file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }
    
    HeapArrayContainer<char> data_buf_container(FILE_READ_BUFFER_LENGTH);
    char* data_buffer = data_buf_container.data();
    size_t bytes_read = 0;
    size_t bytes_read_total = 0;

    vector<char> input_data_log;

    while ((bytes_read = fread(data_buffer, sizeof(data_buffer[0]), FILE_READ_BUFFER_LENGTH, stdin))) {
        if (ferror(stdin) && !feof(stdin)) {
            cleanup(true);

            cout << "error while reading stdin: " + string(strerror(errno)) << endl;

            return Status::ERROR;
        }

        bytes_read_total += bytes_read;

        if (bytes_read_total > MAX_OUT_FILE_SIZE) {
            cleanup(true);

            cout << "input size it too large. Max is " << MAX_OUT_FILE_SIZE << " bytes" << endl;
            
            return Status::ERROR;
        }

        if (DEBUG_ENABLED) {
            input_data_log.insert(input_data_log.end(), data_buffer, data_buffer + bytes_read);
        }

        out_file_stream_.write(data_buffer, bytes_read);

        if (!out_file_stream_.good()) {
            cleanup(true);

            cout << "error while writing data to out file: " + string(strerror(errno)) << endl;
            
            return Status::ERROR;
        }
    }

    if (!bytes_read_total) {
        //empty input
        cleanup(true);

        return Status::OK;
    }
    
    //write header to tail of out file
    if (writeHeaderToOutFile() != 0) {
        cleanup(true);
        
        cout << "error during writing headers to out file" << endl;
        
        return Status::ERROR;
    }
    
    //close out file
    out_file_stream_.close();

    if (!out_file_stream_.good()) {
        cleanup(true);
        
        cout << "error after writing all data to out file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }
    
    //create checksum file for out file
    md5_file_name_ = OUT_FILES_TMP_DIR + out_file_uuid_ + ".md5";
    
    if (createChecksumFile(out_file_path_tmp_, md5_file_name_) != 0) {
        cleanup(true);
        
        cout << "error during writing checksum file" << endl;
        
        return Status::ERROR;
    }
    
    //rename file to final value
    if (renameFile(out_file_path_tmp_, out_file_path_final_) != 0) {
        cleanup(true);
        
        cout << "error during renaming out file" << endl;
        
        return Status::ERROR;
    }
    
    //if DEBUG - log all transfered data to stdout
    if (DEBUG_ENABLED) {
        for (auto b : input_data_log) {
            cout << static_cast<char>(b);
        }
    }
    
    cleanup(false);

    return Status::OK;
}

int InputDataConsumer::writeHeaderToOutFile() {
    string timestamp_mills_str = to_string(timestamp_sec_);
    string channel = AppConfig::getValue(CONFIG_KEY_CHANNEL);

    string header_str = META_KEY_TIMESTAMP + ":" + timestamp_mills_str + ";" 
        + META_KEY_OS + ":linux;"
        + META_KEY_CHANNEL + ":" + (channel != "" ? channel : "default");

    const char* header_c_str = header_str.c_str();

    uint32_t header_len = strlen(header_c_str);

    out_file_stream_.write(header_c_str, static_cast<streamsize>(header_len));
    
    if (!out_file_stream_.good()) {
        cout << "error after writing header string to out file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }
    
    //exactly 4bytes int to be written
    uint32_t header_len_network_byteroder = htonl(header_len);
    out_file_stream_.write(reinterpret_cast<char*>(&header_len_network_byteroder), sizeof(header_len_network_byteroder));

    if (!out_file_stream_.good()) {
        cout << "error after writing header string length to out file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }
    
    return Status::OK;
}

int InputDataConsumer::createChecksumFile(string out_file_path, string md5_file_path) {
    string md5_str = "";
    if (calculateFileMD5(out_file_path, &md5_str) != Status::OK) {
        cout << "error while caculating file md5" << endl;
        
        return Status::ERROR;
    }
    
    ofstream md5_file_stream(md5_file_path, ios::out | ios::binary);

    if (!md5_file_stream) {
        cout << "error while opening md5 file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }
    
    md5_file_stream.write(md5_str.c_str(), md5_str.size());

    if (!md5_file_stream.good()) {
        md5_file_stream.close();
        cleanup(true);

        cout << "error while writing data to md5 file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }

    md5_file_stream.close();

    if (!md5_file_stream.good()) {
        cleanup(true);

        cout << "error while closing md5 file: " + string(strerror(errno)) << endl;
        
        return Status::ERROR;
    }
    
    return Status::OK;
}

void InputDataConsumer::cleanup(bool error) {
    out_file_stream_.close();

    deleteFile(out_file_path_tmp_);
    
    if (error) {
        deleteFile(out_file_path_final_);
        deleteFile(md5_file_name_);
    }
}
