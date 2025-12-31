# Makefile for SMMU Functional Model

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS = 

# Library source files
LIB_SOURCES = tlb.cpp page_table.cpp smmu.cpp smmu_registers.cpp
LIB_OBJECTS = $(LIB_SOURCES:.cpp=.o)

# Headers
HEADERS = smmu_types.h tlb.h page_table.h smmu.h smmu_registers.h

# Executables
TARGET_TEST = test_smmu
TARGET_EXAMPLE = example_advanced

# Default target
all: $(TARGET_TEST) $(TARGET_EXAMPLE)

# Link executables
$(TARGET_TEST): test_smmu.o $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_EXAMPLE): example_advanced.o $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(LIB_OBJECTS) test_smmu.o example_advanced.o $(TARGET_TEST) $(TARGET_EXAMPLE)

# Run tests
test: $(TARGET_TEST)
	./$(TARGET_TEST)

# Run example
example: $(TARGET_EXAMPLE)
	./$(TARGET_EXAMPLE)

# Phony targets
.PHONY: all clean test example

# Dependencies
tlb.o: tlb.cpp tlb.h smmu_types.h
page_table.o: page_table.cpp page_table.h smmu_types.h
smmu.o: smmu.cpp smmu.h smmu_types.h tlb.h page_table.h
smmu_registers.o: smmu_registers.cpp smmu_registers.h
test_smmu.o: test_smmu.cpp smmu.h smmu_registers.h smmu_types.h tlb.h page_table.h
example_advanced.o: example_advanced.cpp smmu.h smmu_registers.h smmu_types.h tlb.h page_table.h
