# DOMES Firmware Host Tests - Simple Makefile
# Build with: make
# Run with: make run

CXX = g++
CC = gcc
CXXFLAGS = -std=c++20 -Wall -Wextra -g -DHOST_BUILD=1
CFLAGS = -Wall -Wextra -g

# Directories
FIRMWARE_ROOT = ../..
TEST_DIR = .
UNITY_DIR = ../unity
MOCK_DIR = ../mocks/esp_idf

# Include paths
INCLUDES = -I$(UNITY_DIR) \
           -I$(MOCK_DIR) \
           -I$(FIRMWARE_ROOT)/main \
           -I$(FIRMWARE_ROOT)/main/config \
           -I$(FIRMWARE_ROOT)/main/platform \
           -I$(FIRMWARE_ROOT)/main/interfaces \
           -I$(FIRMWARE_ROOT)/main/utils

# Source files
UNITY_SRC = $(UNITY_DIR)/unity.c
TEST_SRCS = test_main.cpp test_constants.cpp test_expected.cpp test_color.cpp

# Object files
UNITY_OBJ = unity.o
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

# Output
TARGET = run_tests

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(UNITY_OBJ) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(UNITY_OBJ): $(UNITY_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(UNITY_OBJ) $(TEST_OBJS)
