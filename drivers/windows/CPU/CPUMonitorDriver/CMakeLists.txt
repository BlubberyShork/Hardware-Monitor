cmake_minimum_required(VERSION 3.20)

project(WindowsCPUDriver)

if(NOT MSVC)
    message(FATAL_ERROR "This driver template requires the MSVC compiler on Windows.")
endif()

set(SHARED_HEADERS
    "${CMAKE_SOURCE_DIR}../../../../shared_headers/"
)

# TODO - Change this and WDK Root so its cleaner and not so disgusting and confusing
set(WDK_KIT
    "C:/Program Files (x86)/Windows Kits"
)

set(WDK_ROOT
    "10/Include/10.0.22621.0"
)

set(DRIVER_SOURCES
    cpudriver.c
)

set(MSVC_TOOLS_HEADERS
    "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/include/"
)

set(WDF_KMDF_HEADERS
    "${WDK_KIT}/${WDK_ROOT}/km" 
    "${WDK_KIT}/${WDK_ROOT}/shared"  
    "${WDK_KIT}/10/Include/wdf/kmdf/1.31"
)

add_executable(WindowsCPUDriver 
    ${DRIVER_SOURCES}
)

set_target_properties(WindowsCPUDriver PROPERTIES 
    SUFFIX ".sys" # .sys output ext
    PREFIX ""
)

target_compile_definitions(WindowsCPUDriver PRIVATE _AMD64_)

target_compile_options(WindowsCPUDriver PRIVATE
    #/arch
    /W4          # High warning level
    /WX          # Treat warnings as errors (std for drivers)
    /Gz          # __stdcall calling convention (x86 specific, safe)
    /Kernel      # Inform MSVC compiler to enforce kernel-mode compliance
    /FI "${WDK_KIT}/${WDK_ROOT}/shared/warning.h" # Force include standard warning configurations
)

target_include_directories(WindowsCPUDriver PRIVATE
    ${SHARED_HEADERS}
    ${WDF_KMDF_HEADERS}
    ${MSVC_TOOLS_HEADERS}
)

target_link_options(WindowsCPUDriver PRIVATE
    "/MACHINE:X64"
    "/SUBSYSTEM:NATIVE"          # Marks the binary as a kernel driver
    "/DRIVER"                    
    "/ENTRY:DriverEntry"         # Sets the driver entry point
    "/NODEFAULTLIB"              # Strip user-mode C Runtime (CRT)
    "/INCREMENTAL:NO"            # Disable incremental linking
    "/INTEGRITYCHECK"            # Required for digital signatures / OS loading
    "/MANIFEST:NO"               # Incompatible with /Driver spec -> Disable
)

set(X64_KF_LIBS_DIR ${WDK_KIT}/10/Lib/10.0.22621.0/km/x64)
set(X64_WDF_LIBS_DIR ${WDK_KIT}/10/Lib/wdf/kmdf/x64/1.31)

target_link_libraries(WindowsCPUDriver PRIVATE
    ${X64_KF_LIBS_DIR}/ntoskrnl.lib   
    ${X64_KF_LIBS_DIR}/hal.lib
    ${X64_KF_LIBS_DIR}/BufferOverflowK.lib
    ${X64_WDF_LIBS_DIR}/WdfDriverEntry.lib
    ${X64_WDF_LIBS_DIR}/WdfLdr.lib
    ${X64_KF_LIBS_DIR}/wdmsec.lib
)

string(REGEX REPLACE "/RTC(1|c|s|u)" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}") # Disable Runtime Checks (RTC). 
                                                                                      #  User-space functionality, cant exist in kernel-space
