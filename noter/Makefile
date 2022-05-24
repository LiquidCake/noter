CC=g++
LDLIBS=-lcrypto
#c++ flags
#CXXFLAGS=-DNDEBUG
CXXFLAGS=-std=c++17 -Wall -MD -g -DNDEBUG
#c/c++ preprocessor flags
CPPFLAGS=-Iinclude -I/usr/include/openssl/

all: compile-noter compile-noterd


#noter app
OBJECTS_NOTER=src/noter/noter.o src/noter/input_data_consumer.o src/common/app_config.o src/common/noter_utils.o

compile-noter: $(OBJECTS_NOTER)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) $(OBJECTS_NOTER) $(LDLIBS) -o noter

-include $(OBJECTS_NOTER:.o=.d)


#noter daemon
OBJECTS_NOTERD=src/noterd/noterd.o src/noterd/net_func.o src/common/app_config.o src/common/noter_utils.o

compile-noterd: $(OBJECTS_NOTERD)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) $(OBJECTS_NOTERD) $(LDLIBS) -o noterd

-include $(OBJECTS_NOTERD:.o=.d)


clean:
	rm src/noter/*.o src/noter/*.d src/noterd/*.o src/noterd/*.d src/common/*.o src/common/*.d noter noterd

.DELETE_ON_ERROR:
.PHONY: all compile-noter compile-noterd clean