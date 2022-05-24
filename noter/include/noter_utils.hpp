#ifndef NOTER_NOTER_UTILS
#define NOTER_NOTER_UTILS

#include <string>

enum Status {
    OK = 0,
    ERROR = 1
};

bool fileExists(const std::string file_path);

int deleteFile(const std::string out_file_path);

int renameFile(const std::string old_path, const std::string new_path);

int createDirectories(const std::string full_path);

long getFileSize(std::string file_path);

int calculateFileMD5(const std::string file_path, std::string *out_str);

bool startsWith(std::string str, std::string pref);


template<class T>
class HeapArrayContainer {
public:
    HeapArrayContainer(int size) {
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

#endif //NOTER_NOTER_UTILS
