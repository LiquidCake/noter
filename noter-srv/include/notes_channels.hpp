#ifndef NOTER_NOTES_CHANNELS
#define NOTER_NOTES_CHANNELS

#include <string>
#include <map>
#include <memory>

#include <db_manager.hpp>

struct NoteInfo {
    std::string file_name;
    std::string file_path;
    int file_body_length;
    std::map<std::string, std::string> note_metadata;
};

class NotesChannel {
public:
    NotesChannel() {};
    virtual ~NotesChannel() {};

    NotesChannel(const NotesChannel& other) = delete;
    NotesChannel& operator= (const NotesChannel& other) = delete;

    virtual int sendNote(NoteInfo& note_info) = 0;

    virtual const std::string channelName() = 0;
};

class EmailNotesChannel : public NotesChannel {
public:
    EmailNotesChannel() {};
    ~EmailNotesChannel() override {};

    EmailNotesChannel(const EmailNotesChannel& other) = delete;
    EmailNotesChannel& operator= (const EmailNotesChannel& other) = delete;

    int sendNote(NoteInfo& note_info) override;

    const std::string channelName() override { return CHANNEL_NAME; };

    inline static const std::string CHANNEL_NAME = "email";
};

class DatabseNotesChannel : public NotesChannel {
public:
    DatabseNotesChannel() {};
    ~DatabseNotesChannel() override {};

    DatabseNotesChannel(const DatabseNotesChannel& other) = delete;
    DatabseNotesChannel& operator= (const DatabseNotesChannel& other) = delete;

    int sendNote(NoteInfo& note_info) override;

    const std::string channelName() override { return CHANNEL_NAME; };

    inline static const std::string CHANNEL_NAME = "db";

private:
    DbManager db_manager_;
};

class NotesChannelRegistry {
public:
    NotesChannelRegistry() {
        channel_by_name_[EmailNotesChannel::CHANNEL_NAME] = std::shared_ptr<EmailNotesChannel>(new EmailNotesChannel());
        channel_by_name_[DatabseNotesChannel::CHANNEL_NAME] = std::shared_ptr<DatabseNotesChannel>(new DatabseNotesChannel());

        channel_by_name_["default"] = channel_by_name_[DatabseNotesChannel::CHANNEL_NAME];
    };
    ~NotesChannelRegistry() {};

    NotesChannelRegistry(const NotesChannelRegistry& other) = delete;
    NotesChannelRegistry& operator= (const NotesChannelRegistry& other) = delete;

    std::shared_ptr<NotesChannel> getNotesChannel(std::string channel_name) {
        if (!channel_by_name_.count(channel_name)) {
            throw std::logic_error("unknown channel: " + channel_name);
        }

        return channel_by_name_[channel_name];
    };

private:
    std::map<std::string, std::shared_ptr<NotesChannel>> channel_by_name_;
};

#endif //NOTER_NOTES_CHANNELS
