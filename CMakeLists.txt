# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(tinyusb/src)
# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(volume-up C CXX ASM)

set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/tinyusb/)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(volume-up 
    volume-up.c
    usb_part/usb_part.c
    usb_part/descriptors.c
    bsp.c
    impulsator.c
)
# tinyusb_target_add(volume-up)

pico_set_program_name(volume-up "volume-up")
pico_set_program_version(volume-up "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(volume-up 1)
pico_enable_stdio_usb(volume-up 0)

# target_sources(volume-up
#     PRIVATE
#         )

# Add the standard library to the build
target_link_libraries(volume-up
        pico_stdlib
        pico_bootsel_via_double_reset)


# Add the standard include files to the build
target_include_directories(volume-up PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/usb_part
)

# Add any user requested libraries
target_link_libraries(volume-up 
        pico_cyw43_arch_none
        tinyusb_device
        tinyusb_board
        )

pico_add_extra_outputs(volume-up)

