#ifndef NOTER_INPUT_DATA_CONSUMER
#define NOTER_INPUT_DATA_CONSUMER

#include <iostream>
#include <fstream>
#include <ctime>
#include <map>


class InputDataConsumer {
public:
    explicit InputDataConsumer(std::time_t timestamp_sec) : timestamp_sec_(timestamp_sec) {};
    ~InputDataConsumer() { cleanup(false); };
    
    InputDataConsumer(const InputDataConsumer& other) = delete;
    InputDataConsumer& operator= (const InputDataConsumer& other) = delete;

    int readAndTransferData();

    void cleanup(bool error);

    const char* out_file_path_final() const { return out_file_path_final_.c_str(); }
    const char* out_file_path_tmp() const { return out_file_path_tmp_.c_str(); }
    const char* md5_file_name() const { return md5_file_name_.c_str(); }

private:
    std::time_t timestamp_sec_;
    
    std::ofstream out_file_stream_;

    std::string out_file_uuid_;
    std::string out_file_path_final_;
    std::string out_file_path_tmp_;
    std::string md5_file_name_;

    int writeHeaderToOutFile();
    
    int createChecksumFile(std::string out_file_path, std::string md5_file_path);
};

#endif //NOTER_INPUT_DATA_CONSUMER
