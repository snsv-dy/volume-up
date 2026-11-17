# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# project(audio C CXX)
project(audio C)
# Add executable. Default name is the project name, version 0.1

add_executable(${PROJECT_NAME} audio.c)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../usb_part
)

# Add any user requested libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    pulse
    # usb_protocol_basic # (libusb variant)
    DeviceImpl # (kernel module variant)
    m # math.h
)

# audio.c test begin
# Adding CMocka
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/userspace-driver/cmocka-2.0.2/cmake/Modules")
include(AddCMockaTest)
# message(info ${CMAKE_MODULE_PATH})

add_cmocka_test(audio-test
                SOURCES audio-test.c
                # COMPILE_OPTIONS ${DEFAULT_C_COMPILE_FLAGS} -D_GNU_SOURCE -D_XOPEN_SOURCE=600
                COMPILE_OPTIONS -D_GNU_SOURCE -D_XOPEN_SOURCE=600
                LINK_LIBRARIES cmocka::cmocka pulse m)
target_compile_definitions(audio-test PRIVATE UNIT_TESTING=1)
add_cmocka_test_environment(audio-test)
set_property(TARGET
                 audio-test
             PROPERTY
                 LINK_FLAGS
                    "${DEFAULT_LINK_FLAGS} -Wl,--wrap=read")

# add_executable(${PROJECT_NAME}_test)

target_sources(audio-test
    PRIVATE
        audio.c
)

# # Add the standard include files to the build
target_include_directories(audio-test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        # ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../usb_part
)

# audio.c test end

add_executable(closefiletest closefiletest.c)

add_subdirectory(userspace-driver)

# add_executable(driverUspaceImpl driverUspaceImpl.c)

# # Add the standard include files to the build
# target_include_directories(driverUspaceImpl PRIVATE
#         ${CMAKE_CURRENT_LIST_DIR}
#         ${CMAKE_CURRENT_LIST_DIR}/../usb_part
# )

# Add any user requested libraries
# target_link_libraries(driverUspaceImpl PRIVATE
#     pulse
#     usb_protocol_basic
# )


# add_subdirectory(os/glfw-3.5.1)
# find_package(OpenGL REQUIRED)
# # Gui requires libglfw3-dev to be installed.
# add_executable(gui 
#     ${CMAKE_CURRENT_LIST_DIR}/gui.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/gl.c
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_demo.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_draw.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_tables.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_widgets.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/backends/imgui_impl_glfw.cpp
#     ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master//backends/imgui_impl_opengl3.cpp
# )

# target_link_libraries(gui PRIVATE 
#     glfw
#     GL
#     X11
#     pthread
#     dl
# )

# # get_filename_component(PARENT_DIR ../ ABSOLUTE)
# target_include_directories(gui PRIVATE
#         ${CMAKE_CURRENT_LIST_DIR}
#         ${CMAKE_CURRENT_LIST_DIR}/os/include
#         ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master # for imgui
        
#         # /home/jacek/programy/pico1/volume-up/usb_part/usb_format.h
# )
# message(WARNING ${PARENT_DIR})

add_library(usb_protocol_basic SHARED usb_protocol_basic.c)

target_include_directories(usb_protocol_basic PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../usb_part
)

# find_package(libusb-1.0.0 REQUIRED)
target_link_libraries(usb_protocol_basic PRIVATE
    usb-1.0
)

#   cmake [options] -S <path-to-source> -B <path-to-build>