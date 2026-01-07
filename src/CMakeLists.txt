# src/CMakeLists.txt

# Common library (shared code)
add_library(common STATIC
    common/ticket.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(common PUBLIC
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Back-Office Service
add_executable(backoffice
    backoffice/main.cpp
)

target_link_libraries(backoffice PRIVATE
    common
    httplib::httplib
    Threads::Threads
)

# Ticket Vending Machine
add_executable(tvm
    tvm/main.cpp
)

target_link_libraries(tvm PRIVATE
    common
    httplib::httplib
    ${PAHO_MQTT_CPP}
    ${PAHO_MQTT_C}
    Threads::Threads
)

# Gate Validator
add_executable(gate
    gate/main.cpp
)

target_link_libraries(gate PRIVATE
    common
    httplib::httplib
    ${PAHO_MQTT_CPP}
    ${PAHO_MQTT_C}
    Threads::Threads
)

# Set output directory
set_target_properties(backoffice tvm gate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install targets
install(TARGETS backoffice tvm gate
    RUNTIME DESTINATION bin
)
