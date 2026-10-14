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

# Add executable. Default name is the project name, version 0.1
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        UserspaceDriverImpl.c
)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../usb_part
)