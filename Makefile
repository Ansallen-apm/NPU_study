# Makefile for SMMU Functional Model

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS = 

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
OBJ_DIR = obj
BIN_DIR = bin

# Library source files
LIB_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
LIB_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(LIB_SOURCES))

# Executables
TARGET_TEST = $(BIN_DIR)/test_smmu
TARGET_EXAMPLE = $(BIN_DIR)/example_advanced

# Default target
all: directories $(TARGET_TEST) $(TARGET_EXAMPLE)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Link executables
$(TARGET_TEST): $(OBJ_DIR)/test_smmu.o $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_EXAMPLE): $(OBJ_DIR)/example_advanced.o $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile Library Sources
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile Test Sources
$(OBJ_DIR)/test_smmu.o: $(TEST_DIR)/test_smmu.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/example_advanced.o: $(TEST_DIR)/example_advanced.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Run tests
test: $(TARGET_TEST)
	./$(TARGET_TEST)

# Run example
example: $(TARGET_EXAMPLE)
	./$(TARGET_EXAMPLE)

# Phony targets
.PHONY: all clean test example directories
