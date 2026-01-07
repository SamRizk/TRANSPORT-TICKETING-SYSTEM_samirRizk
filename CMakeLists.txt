cmake_minimum_required(VERSION 3.15)
project(TransportTicketingSystem VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

# Include FetchContent for external dependencies
include(FetchContent)

# JSON library (nlohmann/json)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# HTTP library (cpp-httplib) - header only
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# MQTT C++ (Paho) - System library
find_library(PAHO_MQTT_C paho-mqtt3as)
find_library(PAHO_MQTT_CPP paho-mqttpp3)

if(NOT PAHO_MQTT_C OR NOT PAHO_MQTT_CPP)
    message(STATUS "Paho MQTT not found in system, will need to install:")
    message(STATUS "  sudo apt-get install libpaho-mqtt-dev libpaho-mqttpp-dev")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/common
    ${CMAKE_SOURCE_DIR}/include/backoffice
    ${CMAKE_SOURCE_DIR}/include/tvm
    ${CMAKE_SOURCE_DIR}/include/gate
)

# Add subdirectories
add_subdirectory(src)

# Add tests if enabled
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "==========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "==========================================")
