#include "notes_consumer.hpp"

#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <vector>
#include <map>

#include <noter_utils.hpp>
#include <noter_srv.hpp>
#include <app_config.hpp>

using namespace std;

/* Constants */

extern const string OUT_FILES_TMP_DIR;
extern const string OUT_FILE_TRANSFER_DIR;
extern const string OUT_FILE_ARCHIVED_DIR;
extern const string OUT_FILE_TMP_PREFIX;

extern const string META_KEY_TIMESTAMP;
extern const string META_KEY_OS;
extern const string META_KEY_CHANNEL;

const int CONSUMER_LOOP_DELAY_SEC = 10;

const int TEMP_FILE_NAME_LENGTH = 36;

//1048576000 = 1000 mb
const size_t MAX_OUT_FILE_SIZE = 1048576000L;
const long int MAX_TMP_IDLE_TIME_SEC = 86400L;

const string META_ENTRY_DELIM = ";";
const string META_KEY_VAL_DELIM = ":";

/* Variables */

extern atomic<bool> shutdown_requested;

void watchTempFiles(NotesChannelRegistry& channels_registry) {
    while (true) {
        if (shutdown_requested.load()) {
            break;
        }

        syslog(LOG_DEBUG, "notes consumer thread heartbeat");

        //clear file transfer directory of possible dangling files
        if (createOrClearDirectory(OUT_FILE_TRANSFER_DIR) != Status::OK) {
            syslog(LOG_ERR, "failed to clear transfer files directory");

            continue;
        }

        time_t curr_time_sec = time(0);
        struct stat file_stats;

        for (const auto& entry : filesystem::directory_iterator(OUT_FILES_TMP_DIR)) {
            filesystem::path entry_path = entry.path();
            string file_name = entry_path.filename().string();
            string file_path = entry_path.string();

            if (filesystem::is_directory(entry_path)) {
                continue;
            }

            if (stat(file_path.c_str(), &file_stats) != 0) {
                syslog(LOG_ERR, "error while reading temp file '%s': '%s'", file_path.c_str(), strerror(errno));

                continue;
            }

            if (startsWith(file_name, OUT_FILE_TMP_PREFIX) && curr_time_sec - file_stats.st_ctime > MAX_TMP_IDLE_TIME_SEC) {
                deleteFile(file_path);

                syslog(LOG_INFO, "deleted dangling temp file '%s'", file_path.c_str());

                continue;
            }

            if (file_name.size() != TEMP_FILE_NAME_LENGTH) {
                syslog(LOG_WARNING, "found temp file with bad name: '%s'", file_path.c_str());

                continue;
            }

            size_t file_size = static_cast<long>(file_stats.st_size);

            if (file_size == 0 || file_size > MAX_OUT_FILE_SIZE) {
                syslog(LOG_WARNING, "found temp file with invalid size - %li max is %lui: '%s'", 
                    file_size, MAX_OUT_FILE_SIZE, file_path.c_str());

                continue;
            }

            ifstream temp_file_stream(file_path, ifstream::binary);
            if (!temp_file_stream.is_open() || !temp_file_stream.good()) {
                syslog(LOG_ERR, "failed to read file %s", file_path.c_str());

                continue;
            }

            temp_file_stream.seekg(0, ios::end);
            int file_length = temp_file_stream.tellg();
            temp_file_stream.seekg(0, ios::beg);

            if (!temp_file_stream.good()) {
                syslog(LOG_ERR, "failed to seekg to read file header size %s, %s", file_path.c_str(), strerror(errno));

                continue;
            }

            //reading exactly 4bytes int from the tail of file
            temp_file_stream.seekg(file_length - sizeof(int32_t), ios::beg);
            uint32_t file_header_length_network_byteorder = 0;
            temp_file_stream.read(
                reinterpret_cast<char*>(&file_header_length_network_byteorder), 
                sizeof(file_header_length_network_byteorder)
            );

            if (!temp_file_stream.good()) {
                syslog(LOG_ERR, "failed to read file header size %s: %s", file_path.c_str(), strerror(errno));

                continue;
            }

            uint32_t file_header_length = ntohl(file_header_length_network_byteorder);

            if (file_header_length <= 0) {
                syslog(LOG_ERR, "got bad file header size %s: %i", file_path.c_str(), file_header_length);

                continue;
            }

            int file_body_length = file_length - file_header_length - sizeof(int32_t);

            temp_file_stream.seekg(file_body_length, ios::beg);

            HeapArrayContainer<char> header_arr(file_header_length + 1);
            header_arr.data()[file_header_length] = 0;
            temp_file_stream.read(header_arr.data(), file_header_length);

            string header_str = string(header_arr.data());

            if (!temp_file_stream.good()) {
                syslog(LOG_ERR, "failed to read file header %s: %s", file_path.c_str(), strerror(errno));

                continue;
            }

            temp_file_stream.close();

            if (!temp_file_stream.good()) {
                syslog(LOG_ERR, "failed to close file %s: %s", file_path.c_str(), strerror(errno));

                continue;
            }

            struct NoteInfo note_info;
            note_info.file_name = file_name;
            note_info.file_path = file_path;
            note_info.file_body_length = file_body_length;
            note_info.note_metadata = parseNoteMetadata(header_str);

            shared_ptr<NotesChannel> channel_ptr = channels_registry.getNotesChannel(note_info.note_metadata[META_KEY_CHANNEL]);

            if (channel_ptr->sendNote(note_info) == Status::OK) {
                syslog(LOG_INFO, "successfully processed note '%s' via channel '%s'", 
                    note_info.file_path.c_str(), channel_ptr->channelName().c_str());

                if (AppConfig::getValue(CONFIG_DELETE_NOTE_AFTER_PROCESSING) == "true") {
                    if (deleteFile(note_info.file_path) != Status::OK) {
                        syslog(LOG_ERR, "failed to delete note file '%s' after processing", note_info.file_path.c_str());
                    }
                } else {
                    if (createDirectories(OUT_FILE_ARCHIVED_DIR) != Status::OK) {
                        syslog(LOG_ERR, "failed to create archive directory");
                    } else if (renameFile(note_info.file_path, OUT_FILE_ARCHIVED_DIR + file_name) != Status::OK) {
                        syslog(LOG_ERR, "failed to move note file '%s' after processing", note_info.file_path.c_str());
                    }
                }
            } else {
                syslog(LOG_ERR, "failed to send note '%s' via channel '%s'", 
                    note_info.file_path.c_str(), channel_ptr->channelName().c_str());
            }
        }

        sleep(CONSUMER_LOOP_DELAY_SEC);
    }
}

map<string, string> parseNoteMetadata(string header_str) {
    map<string, string> metadata_map;
    vector<string> meta_entries = split_string_by_delim(header_str, META_ENTRY_DELIM);

    for (auto entry : meta_entries) {

        vector<string> key_val = split_string_by_delim(entry, META_KEY_VAL_DELIM);

        if (key_val.size() == 2) {
            metadata_map[key_val[0]] = key_val[1];
        }
    }

    return metadata_map;
}
