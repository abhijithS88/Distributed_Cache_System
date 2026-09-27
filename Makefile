CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
BUILD_DIR := build
HOST ?= 127.0.0.1
PORT ?= 4005

SERVER_SOURCES := server.cpp \
	command_parser/command_parser.cpp \
	process_request/process_request.cpp \
	hashtable/hashtable.cpp
SERVER_HEADERS := command_parser/command_parser.h \
	process_request/process_request.h \
	hashtable/hashtable.h \
	container_of/container_of.h

.PHONY: all server client run clean

all: server client

server: $(BUILD_DIR)/server

client: $(BUILD_DIR)/client

$(BUILD_DIR)/server: $(SERVER_SOURCES) $(SERVER_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(SERVER_SOURCES) $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/client: client.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ client.cpp $(LDFLAGS) $(LDLIBS)

run: server
	$(BUILD_DIR)/server $(HOST) $(PORT)

clean:
	rm -rf $(BUILD_DIR)