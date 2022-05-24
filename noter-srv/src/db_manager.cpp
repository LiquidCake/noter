/**
 * Noter code is not excepted to query DB often so no connection pooling implemented
*/

#include "db_manager.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <mysql/jdbc.h>
#pragma GCC diagnostic pop

#include <syslog.h>

#include "noter_utils.hpp"
#include "app_config.hpp"

using namespace std;


int DbManager::executeQuery(string query, unique_ptr<sql::ResultSet> result_set_p) {
    const string url = AppConfig::getValue(CONFIG_DB_URL);
    const string database = AppConfig::getValue(CONFIG_DB_DATABASE);
    const string user = AppConfig::getValue(CONFIG_DB_USERNAME);
    const string pass = AppConfig::getValue(CONFIG_DB_PASSWORD);

    if (url == "" 
        || database == ""
        || user == ""
        || pass == ""
    ) {
        syslog(LOG_ERR, "bad DB configs");
        return -1;
    }

    try {
        sql::Driver *driver = sql::mysql::get_mysql_driver_instance();

        std::unique_ptr<sql::Connection> con(driver->connect(url, user, pass));
        con->setSchema(database);

        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery(query));

        result_set_p = move(rs);
    } catch (sql::SQLException &e) {
        syslog(LOG_ERR, "Exception at %s:%d - ERROR: %s, ERR CODE: %i, SQL STATE: %s",
            __FILE__, __LINE__, e.what(), e.getErrorCode(), e.getSQLState().c_str());

        return Status::ERROR;
    }

    return Status::OK;
}

int DbManager::executeUpdateStrStrBlob(string query, string str_1, string str_2, istream *blob_stream) {
    const string url = AppConfig::getValue(CONFIG_DB_URL);
    const string database = AppConfig::getValue(CONFIG_DB_DATABASE);
    const string user = AppConfig::getValue(CONFIG_DB_USERNAME);
    const string pass = AppConfig::getValue(CONFIG_DB_PASSWORD);

    if (url == "" 
        || database == ""
        || user == ""
        || pass == ""
    ) {
        syslog(LOG_ERR, "bad DB configs");
        return -1;
    }

    try {
        sql::Driver *driver = sql::mysql::get_mysql_driver_instance();

        std::unique_ptr<sql::Connection> con(driver->connect(url, user, pass));
        con->setSchema(database);

        std::unique_ptr<sql::PreparedStatement> stmt(con->prepareStatement(query));
        stmt->setString(1, str_1);
        stmt->setString(2, str_2);
        stmt->setBlob(3, blob_stream);

        return stmt->executeUpdate();
    } catch (sql::SQLException &e) {
        syslog(LOG_ERR, "Exception at %s:%d - ERROR: %s, ERR CODE: %i, SQL STATE: %s",
            __FILE__, __LINE__, e.what(), e.getErrorCode(), e.getSQLState().c_str());

        return -1;
    }

    return 0;
}