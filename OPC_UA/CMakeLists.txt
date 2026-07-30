# OPC_UA/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(OpcUaServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(UA_ENABLE_AMALGAMATION OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
set(UA_NAMESPACE_ZERO FULL CACHE STRING "" FORCE)
set(UA_ENABLE_ENCRYPTION MBEDTLS CACHE STRING "" FORCE)

add_subdirectory(
    ${CMAKE_SOURCE_DIR}/../third_party/open62541pp
    ${CMAKE_BINARY_DIR}/open62541pp
)

add_executable(OpcUaServer
    main.cpp
    server.cpp
    AccessControlCustom.cpp
    test_client.cpp
    client.cpp
)

target_include_directories(OpcUaServer PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(OpcUaServer PRIVATE
    open62541pp::open62541pp
)
