# control_center/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(ControlCenter LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UA_ENABLE_AMALGAMATION OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
set(UA_NAMESPACE_ZERO FULL CACHE STRING "" FORCE)
set(UA_ENABLE_ENCRYPTION MBEDTLS CACHE STRING "" FORCE)

add_subdirectory(
    ${CMAKE_SOURCE_DIR}/../third_party/open62541pp
    ${CMAKE_BINARY_DIR}/open62541pp
)

add_executable(ControlCenter
    main.cpp
    ControlCenterClient.cpp
    DisplayGrid.cpp
    ${CMAKE_SOURCE_DIR}/../OPC_UA/CustomClient.cpp
    ${CMAKE_SOURCE_DIR}/../OPC_UA/utils.cpp
    ${CMAKE_SOURCE_DIR}/../OPC_UA/ClientQueue.cpp
)

target_include_directories(ControlCenter PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../OPC_UA
)

target_link_libraries(ControlCenter PRIVATE
    open62541pp::open62541pp
)
