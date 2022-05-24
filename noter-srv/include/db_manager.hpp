#ifndef NOTER_DB_MANAGER
#define NOTER_DB_MANAGER

#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <mysql/jdbc.h>
#pragma GCC diagnostic pop

class DbManager {
public:
    DbManager() {};
    ~DbManager() {};

    DbManager(const DbManager& other) = delete;
    DbManager& operator= (const DbManager& other) = delete;

    int executeQuery(std::string query, std::unique_ptr<sql::ResultSet> result_set_p);
    int executeUpdateStrStrBlob(std::string query, std::string str_1, std::string str_2, std::istream *blob_stream);
};

#endif //NOTER_DB_MANAGER