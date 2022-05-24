#include "input_data_consumer.hpp"

#include <unistd.h>

#include <iostream>
#include <csignal>
#include <ctime>
#include <memory>
#include <fstream>
#include <sstream>
#include <map>

#include "noter_utils.hpp"
#include "app_config.hpp"

using namespace std;

unique_ptr<InputDataConsumer> input_data_consumer;


void registerSignalHandlers();

void sigHandler(int sig_num);

int main() {
    AppConfig::init();

    unique_ptr<InputDataConsumer> consumer(new InputDataConsumer(time(nullptr)));
    input_data_consumer = move(consumer);

    registerSignalHandlers();

    return input_data_consumer->readAndTransferData();
}

void registerSignalHandlers() {
    signal(SIGINT, sigHandler);
    signal(SIGABRT, sigHandler);
    signal(SIGTERM, sigHandler);
}

void sigHandler(int sig_num) {
    unlink(input_data_consumer->out_file_path_tmp());
    unlink(input_data_consumer->out_file_path_final());
    unlink(input_data_consumer->md5_file_name());

    exit(sig_num);
}
