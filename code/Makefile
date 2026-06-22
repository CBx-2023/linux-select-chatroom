CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -Iinclude
LDFLAGS ?=

BUILD_DIR := build

SERVER_TARGET := server
CLIENT_TARGET := client

SERVER_OBJECTS := \
	$(BUILD_DIR)/server.o \
	$(BUILD_DIR)/client_manager.o \
	$(BUILD_DIR)/protocol.o \
	$(BUILD_DIR)/logger.o

CLIENT_OBJECTS := \
	$(BUILD_DIR)/client.o \
	$(BUILD_DIR)/input_handler.o \
	$(BUILD_DIR)/display.o

.PHONY: all clean

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(SERVER_TARGET) $(CLIENT_TARGET)
