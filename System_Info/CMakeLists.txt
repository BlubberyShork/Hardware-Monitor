cmake_minimum_required(VERSION 3.20)
project(SystemInfo LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UA_ENABLE_AMALGAMATION OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
set(UA_NAMESPACE_ZERO FULL CACHE STRING "" FORCE)

add_subdirectory(
    ${CMAKE_SOURCE_DIR}/../third_party/open62541pp
    ${CMAKE_BINARY_DIR}/open62541pp
)

# =============================================================================
# SHARED SOURCES -- built on every platform, no OS-specific headers/APIs.
# =============================================================================
set(SHARED_SOURCES
    output_generator/OutputHandler.cpp
    hardware/HardwareDevice.cpp
    hardware/IHardwareDevicePool.cpp
    ${CMAKE_SOURCE_DIR}/../OPC_UA/ClientQueue.cpp
    ${CMAKE_SOURCE_DIR}/../OPC_UA/client.cpp
    # OPC_UA/AccessControlCustom.cpp
    # hardware/DeviceSensor.h has no .cpp -- header-only, nothing to list here
)

# =============================================================================
# WINDOWS-ONLY SOURCES -- WMI/COM, ADLX/ADL/NVAPI, DXGI, driver client, etc.
# Only ever added to the build when WIN32 is true.
# =============================================================================
set(WINDOWS_SOURCES
    System_Info.cpp
    HardwareManager.cpp
    ThreadManager.cpp
    driver_client/DriverClient.cpp
    wmi/ComManager.cpp
    wmi/WbemManager.cpp
    wmi/GraphicsProcessor.cpp
    wmi/HardwareQueries.cpp
    wmi/motherboard.cpp
    wmi/processor.cpp
    wmi/projutils.cpp
    wmi/storagedevice.cpp
    hardware/GPU/DxgiHandler.cpp
    hardware/CPU/CPULiveMetrics.cpp
    hardware/pipeline_workers/CPUPipelineWorker.cpp
    hardware/pipeline_workers/GPUPipelineWorker.cpp
    hardware/GPU/AMD/AMDPool.cpp
    hardware/GPU/AMD/AMDLiveGPUMetrics.cpp
    hardware/GPU/AMD/Interop/ADLX.cpp
    hardware/GPU/AMD/Interop/ADL.cpp
    hardware/GPU/NVIDIA/NvidiaPool.cpp
    hardware/GPU/NVIDIA/NvidiaLiveGPUMetrics.cpp
    ${CMAKE_SOURCE_DIR}/../third_party/adlx/SDK/ADLXHelper/Windows/Cpp/ADLXHelper.cpp
    ${CMAKE_SOURCE_DIR}/../third_party/adlx/SDK/Platform/Windows/WinAPIs.cpp
)

set(WINDOWS_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/shared_headers
    ${CMAKE_SOURCE_DIR}/../shared_headers
    ${CMAKE_SOURCE_DIR}/third_party/adlx/SDK/Include
    ${CMAKE_SOURCE_DIR}/../third_party/adlx/SDK/Include
    ${CMAKE_SOURCE_DIR}/third_party/adlx/SDK/ADLXHelper/Windows/Cpp
    ${CMAKE_SOURCE_DIR}/../third_party/adlx/SDK/ADLXHelper/Windows/Cpp
    ${CMAKE_SOURCE_DIR}/third_party/adl/include
    ${CMAKE_SOURCE_DIR}/../third_party/adl/include
    ${CMAKE_SOURCE_DIR}/third_party/nvapi/amd64
    ${CMAKE_SOURCE_DIR}/../third_party/nvapi/amd64
    ${CMAKE_SOURCE_DIR}/third_party/nvapi
    ${CMAKE_SOURCE_DIR}/../third_party/nvapi
)

set(WINDOWS_LINK_DIRS
    ${CMAKE_SOURCE_DIR}/third_party/nvapi/amd64
    ${CMAKE_SOURCE_DIR}/../third_party/nvapi/amd64
    ${CMAKE_SOURCE_DIR}/third_party/adlx/SDK/ADLXHelper/Windows/Cpp
    ${CMAKE_SOURCE_DIR}/../third_party/adlx/SDK/ADLXHelper/Windows/Cpp
    ${CMAKE_SOURCE_DIR}/third_party/adlx/SDK/Include
    ${CMAKE_SOURCE_DIR}/../third_party/adlx/SDK/Include
    ${CMAKE_SOURCE_DIR}/third_party/adl/include
    ${CMAKE_SOURCE_DIR}/../third_party/adl/include
)

set(WINDOWS_LINK_LIBS
    dxgi
    nvapi64
)

# =============================================================================
# LINUX-ONLY SOURCES -- placeholder. Fill in as the Linux pipeline is built.
# Only ever added to the build when on Linux.
# =============================================================================
set(LINUX_SOURCES
    # linux/HardwareManager.cpp
    # linux/... etc
)

set(LINUX_INCLUDE_DIRS
    # linux include paths go here
)

set(LINUX_LINK_LIBS
    # linux libraries go here
)

# =============================================================================
# Assemble the actual target based on platform. Only one branch below is
# ever active, so the other platform's headers/libs are never referenced.
# =============================================================================
if(WIN32)
    set(PLATFORM_SOURCES     ${WINDOWS_SOURCES})
    set(PLATFORM_INCLUDES    ${WINDOWS_INCLUDE_DIRS})
    set(PLATFORM_LINK_DIRS   ${WINDOWS_LINK_DIRS})
    set(PLATFORM_LINK_LIBS   ${WINDOWS_LINK_LIBS})
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_SOURCES     ${LINUX_SOURCES})
    set(PLATFORM_INCLUDES    ${LINUX_INCLUDE_DIRS})
    set(PLATFORM_LINK_DIRS   "")
    set(PLATFORM_LINK_LIBS   ${LINUX_LINK_LIBS})
else()
    message(FATAL_ERROR "Unsupported platform for SystemInfo.")
endif()

add_executable(SystemInfo
    System_Info.cpp
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_include_directories(SystemInfo PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${PLATFORM_INCLUDES}
)

target_link_directories(SystemInfo PRIVATE
    ${PLATFORM_LINK_DIRS}
)

target_link_libraries(SystemInfo PRIVATE
    ${PLATFORM_LINK_LIBS}
    open62541pp::open62541pp
)

if(WIN32)
    target_compile_definitions(SystemInfo PRIVATE
        WIN32
        _CONSOLE
        UNICODE
        _UNICODE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
endif()

if(MSVC)
    target_compile_options(SystemInfo PRIVATE
        /W3
        /sdl
        /permissive-
    )
endif()
