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

# Target executables
TARGET = $(BUILD_DIR)/atlasdb
BENCH_TARGET = $(BUILD_DIR)/atlasdb_bench

# Source and object files
COMMON_SRCS = $(filter-out $(SRC_DIR)/main.cpp $(SRC_DIR)/benchmark_main.cpp, $(wildcard $(SRC_DIR)/*.cpp))
COMMON_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(COMMON_SRCS))
MAIN_OBJ = $(OBJ_DIR)/main.o
BENCH_MAIN_OBJ = $(OBJ_DIR)/benchmark_main.o

# Default rule
all: $(TARGET)

# Link the executables
$(TARGET): $(COMMON_OBJS) $(MAIN_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(COMMON_OBJS) $(MAIN_OBJ) -o $(TARGET) $(LDFLAGS)

$(BENCH_TARGET): $(COMMON_OBJS) $(BENCH_MAIN_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(COMMON_OBJS) $(BENCH_MAIN_OBJ) -o $(BENCH_TARGET) $(LDFLAGS)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the database
run: all
	@mkdir -p $(LOG_DIR)
	./$(TARGET)

bench: $(BENCH_TARGET)
	@mkdir -p $(LOG_DIR)
	./$(BENCH_TARGET) --profile=quick

bench-dev: $(BENCH_TARGET)
	@mkdir -p $(LOG_DIR)
	./$(BENCH_TARGET) --profile=dev

bench-large: $(BENCH_TARGET)
	@mkdir -p $(LOG_DIR)
	./$(BENCH_TARGET) --profile=large

bench-stress: $(BENCH_TARGET)
	@mkdir -p $(LOG_DIR)
	./$(BENCH_TARGET) --profile=stress

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Debugging: show variables
print:
	@echo "COMMON_SRCS: $(COMMON_SRCS)"
	@echo "COMMON_OBJS: $(COMMON_OBJS)"
	@echo "TARGET: $(TARGET)"
	@echo "BENCH_TARGET: $(BENCH_TARGET)"

.PHONY: all clean run bench bench-dev bench-large bench-stress print
