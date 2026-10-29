# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# Dla CXX spróbuj opcji buildroota 
# Symbol: BR2_TOOLCHAIN_BUILDROOT_CXX [=n]                                                            │  
#   │ Type  : bool                                                                                        │  
#   │ Prompt: Enable C++ support
# project(UserspaceDriverImpl C CXX)
project(UserspaceDriverImpl C)

add_subdirectory(cmocka-2.0.2)
# include(cmocka-2.0.2)
# Add executable. Default name is the project name, version 0.1
add_executable(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_MAIN)

target_sources(${PROJECT_NAME}
    PRIVATE
    UserspaceDriverImpl.c
)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../../usb_part
)

# Program as a library 
add_library(DeviceImpl STATIC)

target_sources(DeviceImpl
    PRIVATE
    UserspaceDriverImpl.c
)

target_include_directories(DeviceImpl PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../../usb_part
)

# Adding CMocka
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmocka-2.0.2/cmake/Modules")
include(AddCMockaTest)
# message(info ${CMAKE_MODULE_PATH})

add_cmocka_test(iotest
                SOURCES test/iotest.c
                # COMPILE_OPTIONS ${DEFAULT_C_COMPILE_FLAGS} -D_GNU_SOURCE -D_XOPEN_SOURCE=600
                COMPILE_OPTIONS -D_GNU_SOURCE -D_XOPEN_SOURCE=600
                LINK_LIBRARIES cmocka::cmocka)
target_compile_definitions(iotest PRIVATE UNIT_TESTING=1)
add_cmocka_test_environment(iotest)
set_property(TARGET
                 iotest
             PROPERTY
                 LINK_FLAGS
                    "${DEFAULT_LINK_FLAGS} -Wl,--wrap=read")

# add_executable(${PROJECT_NAME}_test)

target_sources(iotest
    PRIVATE
        UserspaceDriverImpl.c
)

# # Add the standard include files to the build
target_include_directories(iotest PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../usb_part
)