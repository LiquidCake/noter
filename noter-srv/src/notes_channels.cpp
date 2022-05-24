#include "notes_channels.hpp"

#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <fstream>

#include "noter_utils.hpp"
#include "email_sender.hpp"
#include "db_manager.hpp"

using namespace std;

extern const string META_KEY_TIMESTAMP;
extern const string META_KEY_OS;
extern const string META_KEY_CHANNEL;

extern const string OUT_FILE_TRANSFER_DIR;

int EmailNotesChannel::sendNote(NoteInfo& note_info) {
    syslog(LOG_DEBUG, "EmailNotesChannel::sendNote %s", note_info.file_path.c_str());

    ifstream temp_file_stream(note_info.file_path, ifstream::binary);
    if (!temp_file_stream.is_open() || !temp_file_stream.good()) {
        syslog(LOG_ERR, "EmailNotesChannel: failed to read file %s", note_info.file_path.c_str());

        return Status::ERROR;
    }

    HeapArrayContainer<char> file_body(note_info.file_body_length + 1);
    file_body.data()[note_info.file_body_length] = 0;

    temp_file_stream.read(file_body.data(), note_info.file_body_length);

    if (!temp_file_stream.good()) {
        syslog(LOG_ERR, "EmailNotesChannel: failed to read file body %s: %s", 
            note_info.file_path.c_str(), strerror(errno));

        return Status::ERROR;
    }

    string subject = "Note from " + timestampToString(stol(note_info.note_metadata[META_KEY_TIMESTAMP]));
    string text_payload = string(file_body.data());

    if (EmailSender::sendEmail(subject, text_payload) != 0) {
        return Status::ERROR;
    }

    return Status::OK;
}

int DatabseNotesChannel::sendNote(NoteInfo& note_info) {
    syslog(LOG_DEBUG, "DatabseNotesChannel::sendNote %s", note_info.file_path.c_str());

    //copy file to transfer dir to be able to do whatever we want with copy
    string transfer_file_copy_path = OUT_FILE_TRANSFER_DIR + note_info.file_name;
    if (copyFile(note_info.file_path, transfer_file_copy_path) != Status::OK) {
        syslog(LOG_ERR, "DatabseNotesChannel: failed to copy file to transfer dir: %s", 
            note_info.file_path.c_str());

        return Status::ERROR;
    }

    int fd = open(transfer_file_copy_path.c_str(), O_RDWR);
    if (fd == -1) {
        syslog(LOG_ERR, "DatabseNotesChannel: failed to open transfer file %s: %s", 
            note_info.file_path.c_str(), strerror(errno));

        return Status::ERROR;
    }

    if (ftruncate(fd, note_info.file_body_length) != 0) {
        syslog(LOG_ERR, "DatabseNotesChannel: failed to truncate transfer file %s: %s", 
            note_info.file_path.c_str(), strerror(errno));
        close(fd);

        return Status::ERROR;
    }

    close(fd);

    ifstream transfer_file_stream(transfer_file_copy_path, ifstream::binary);
    if (!transfer_file_stream.is_open() || !transfer_file_stream.good()) {
        syslog(LOG_ERR, "DatabseNotesChannel: failed to open transfer file for DB transfer %s",
            transfer_file_copy_path.c_str());

        return Status::ERROR;
    }

    string db_update_query = "INSERT INTO note (note_id, note_meta, blob_content) VALUES (?, ?, ?);";
    int rows_to_insert = 1;

    if (db_manager_.executeUpdateStrStrBlob(
            db_update_query,
            note_info.file_name, 
            stringMapToJson(note_info.note_metadata), 
            &transfer_file_stream
        ) != rows_to_insert) {
        syslog(LOG_ERR, "DatabseNotesChannel: failed to insert note into DB %s",
            transfer_file_copy_path.c_str());

        return Status::ERROR;
    }

    return Status::OK;
}
