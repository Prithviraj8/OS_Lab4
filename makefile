# Makefile for compiling and running the scheduling simulation

# Compiler settings
CC=g++
CFLAGS=-std=c++11 -Wall

# Define the executable file
TARGET=iosched

# Source files
SOURCES= iosched.cpp globals.cpp

# Object files
OBJECTS=$(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Custom command to clean the build
clean:
	rm -f $(TARGET) $(OBJECTS)

# Custom command to run the program with specified file paths
# Usage: make run SCHEDULER_SPEC="-sF" FILE="input_file_path" FILE2="rand_file_path"
# Define default values for variables if not provided
ALGO ?= 'f'
OPS ?= 'v'
FILE ?= 'file'
# Custom command to run the program with specified file paths
run:
	./$(TARGET) -s$(ALGO) -$(OPS) $(FILE)

.PHONY: all clean run
