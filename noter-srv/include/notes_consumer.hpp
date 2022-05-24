#ifndef NOTER_NOTES_CONSUMER
#define NOTER_NOTES_CONSUMER

#include <string>
#include <map>

#include "notes_channels.hpp"

void watchTempFiles(NotesChannelRegistry& channels_registry);

std::map<std::string, std::string> parseNoteMetadata(std::string header_str);

#endif //NOTER_NOTES_CONSUMER