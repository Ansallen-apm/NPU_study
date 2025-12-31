cmake_minimum_required(VERSION 3.10)
project(SMMU_FunctionalModel VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# ============================================================================
# Options
# ============================================================================

option(BUILD_WITH_SYSTEMC "Build with SystemC TLM support" OFF)

# ============================================================================
# Source files
# ============================================================================

set(SMMU_SOURCES
    tlb.cpp
    page_table.cpp
    smmu.cpp
    smmu_registers.cpp
)

set(SMMU_HEADERS
    smmu_types.h
    tlb.h
    page_table.h
    smmu.h
    smmu_registers.h
)

set(SMMU_TLM_HEADERS
    tlm_types.h
    smmu_tlm_target.h
    smmu_tlm_initiator.h
    smmu_tlm_wrapper.h
)

# ============================================================================
# SMMU Library
# ============================================================================

add_library(smmu_lib STATIC ${SMMU_SOURCES} ${SMMU_HEADERS})

# ============================================================================
# Basic Executables (no SystemC required)
# ============================================================================

# Test executable
add_executable(test_smmu test_smmu.cpp)
target_link_libraries(test_smmu smmu_lib)

# Advanced example
add_executable(example_advanced example_advanced.cpp)
target_link_libraries(example_advanced smmu_lib)

# ============================================================================
# SystemC TLM Support (optional)
# ============================================================================

if(BUILD_WITH_SYSTEMC)
    # Find SystemC
    set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
    if(NOT SYSTEMC_HOME)
        set(SYSTEMC_HOME "/usr/local/systemc-2.3.3")
    endif()
    
    message(STATUS "SystemC home: ${SYSTEMC_HOME}")
    
    # Check if SystemC exists
    if(EXISTS "${SYSTEMC_HOME}/include/systemc.h")
        # Add SystemC include directory
        include_directories(${SYSTEMC_HOME}/include)
        
        # Add SystemC library directory
        link_directories(${SYSTEMC_HOME}/lib-linux64)
        
        # SystemC TLM test executable
        add_executable(test_smmu_tlm test_smmu_tlm.cpp)
        target_link_libraries(test_smmu_tlm smmu_lib systemc)
        
        # Set RPATH for SystemC library
        set_target_properties(test_smmu_tlm PROPERTIES
            INSTALL_RPATH "${SYSTEMC_HOME}/lib-linux64"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
        
        message(STATUS "SystemC TLM support enabled")
        
        # Install TLM test
        install(TARGETS test_smmu_tlm
            RUNTIME DESTINATION bin
        )
        
        # Install TLM headers
        install(FILES ${SMMU_TLM_HEADERS}
            DESTINATION include/smmu
        )
    else()
        message(WARNING "SystemC not found at ${SYSTEMC_HOME}")
        message(WARNING "Set SYSTEMC_HOME environment variable or disable BUILD_WITH_SYSTEMC")
    endif()
endif()

# ============================================================================
# Install targets
# ============================================================================

install(TARGETS smmu_lib test_smmu example_advanced
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${SMMU_HEADERS}
    DESTINATION include/smmu
)

# ============================================================================
# Testing
# ============================================================================

enable_testing()
add_test(NAME smmu_basic_tests COMMAND test_smmu)
add_test(NAME smmu_advanced_example COMMAND example_advanced)

if(BUILD_WITH_SYSTEMC AND TARGET test_smmu_tlm)
    add_test(NAME smmu_tlm_test COMMAND test_smmu_tlm)
endif()

# ============================================================================
# Print configuration
# ============================================================================

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "SystemC support: ${BUILD_WITH_SYSTEMC}")

if(BUILD_WITH_SYSTEMC)
    message(STATUS "SystemC home: ${SYSTEMC_HOME}")
endif()
