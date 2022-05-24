#ifndef NOTER_SRV_NOTER_UTILS
#define NOTER_SRV_NOTER_UTILS

#include <string>
#include <vector>
#include <map>

enum Status {
    OK = 0,
    ERROR = 1
};

bool fileExists(const std::string file_path);

int deleteFile(const std::string out_file_path);

int renameFile(const std::string old_path, const std::string new_path);

int createDirectories(const std::string full_path);

int copyFile(const std::string orig_path, const std::string dest_path);

int createOrClearDirectory(std::string dir_path);

long getFileSize(std::string file_path);

int calculateFileMD5(const std::string file_path, std::string *out_str);

bool startsWith(std::string str, std::string pref);

bool endsWith(std::string str, std::string ending);

std::vector<std::string> split_string_by_delim(std::string str, std::string delim);

std::string timestampToString(long timestamp);

std::string stringMapToJson(std::map<std::string, std::string>& string_map);

template<class T>
class HeapArrayContainer {
public:
    HeapArrayContainer(int size = 0) {
        arr_ = new T[size];
    };
    ~HeapArrayContainer() {
        delete[] arr_;
    }
    
    HeapArrayContainer(const HeapArrayContainer& other) = delete;
    HeapArrayContainer& operator= (const HeapArrayContainer& other) = delete;
    
    T* data() { return arr_; }
private:
    T* arr_;
};

#endif //NOTER_SRV_NOTER_UTILS
