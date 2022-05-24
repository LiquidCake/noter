CC=g++
LDLIBS=-lcrypto -lcurl -lmysqlcppconn
#c++ flags
#CXXFLAGS=-DNDEBUG
CXXFLAGS=-std=c++17 -pthread -Wall -MD -g -DNDEBUG
#c/c++ preprocessor flags
CPPFLAGS=-Iinclude -I/usr/include/openssl/ -I/usr/include/mysql-cppconn-8/

OBJECTS=src/noter_srv.o src/notes_consumer.o src/notes_channels.o src/net_func.o src/noter_utils.o src/email_sender.o src/db_manager.o src/app_config.o

all: compile

compile: $(OBJECTS)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) $(OBJECTS) $(LDLIBS) -o noter-srv

-include $(OBJECTS:.o=.d)

clean:
	rm src/*.o src/*.d noter-srv

.DELETE_ON_ERROR:
.PHONY: all compile clean