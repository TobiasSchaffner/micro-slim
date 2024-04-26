# Define the compiler to use
CC=gcc

# Define any compile-time flags
CFLAGS=-Wall -g

# Define the target executable name
TARGET=micro-slim

# Default target: build the executable
all: $(TARGET)

# Rule for building the target executable
$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

# Rule for cleaning up generated files
clean:
	rm -f $(TARGET)

# Rule to run the program (optional)
run: $(TARGET)
	./$(TARGET)
