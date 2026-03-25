# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LOG_DIR = logs

# Target executable
TARGET = $(BUILD_DIR)/atlasdb

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Default rule
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the database
run: all
	@mkdir -p $(LOG_DIR)
	./$(TARGET) --profile=quick

bench-dev: all
	@mkdir -p $(LOG_DIR)
	./$(TARGET) --profile=dev

bench-large: all
	@mkdir -p $(LOG_DIR)
	./$(TARGET) --profile=large

bench-stress: all
	@mkdir -p $(LOG_DIR)
	./$(TARGET) --profile=stress

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Debugging: show variables
print:
	@echo "SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"
	@echo "TARGET: $(TARGET)"

.PHONY: all clean run bench-dev bench-large bench-stress print
