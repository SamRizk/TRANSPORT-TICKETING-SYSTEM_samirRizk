# tests/CMakeLists.txt

# Find Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, fetching from GitHub...")
    
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
endif()

# Enable testing
enable_testing()

# Unit test executable
add_executable(test_ticket
    unit/test_ticket.cpp
)

target_link_libraries(test_ticket PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(test_ticket PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add test to CTest
add_test(NAME TicketUnitTests COMMAND test_ticket)

# Set test properties
set_tests_properties(TicketUnitTests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

# Integration test script
add_test(
    NAME IntegrationTests
    COMMAND ${CMAKE_SOURCE_DIR}/tests/integration/integration_test.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 120
    LABELS "integration"
)

# Custom test target
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_ticket
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests..."
)

# Test with verbose output
add_custom_target(run_tests_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_ticket
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests with verbose output..."
)

message(STATUS "Tests configured:")
message(STATUS "  - Unit tests: test_ticket")
message(STATUS "  - Integration tests: integration_test.sh")
message(STATUS "Run with: cd build && ctest")
