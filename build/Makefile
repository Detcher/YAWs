CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g

TARGET = server
OBJS = ../code/log/*.cpp ../code/pool/*.cpp ../code/timer/*.cpp \
       ../code/http/*.cpp ../code/server/*.cpp \
       ../code/buffer/*.cpp ../code/main.cpp

# Create the bin directory if it doesn't exist
BIN_DIR = ../bin
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# The default target
all: $(BIN_DIR) $(TARGET)

# Link the object files into the final executable
$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(BIN_DIR)/$(TARGET) -pthread -lmysqlclient

# Clean up build artifacts
clean:
	rm -rf $(BIN_DIR)/$(TARGET)
