# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(audio C CXX)
# Add executable. Default name is the project name, version 0.1

add_executable(${PROJECT_NAME} audio.c)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    pulse
)


add_subdirectory(os/glfw-3.5.1)
find_package(OpenGL REQUIRED)
# Gui requires libglfw3-dev to be installed.
add_executable(gui 
    ${CMAKE_CURRENT_LIST_DIR}/gui.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/gl.c
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_demo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_draw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_tables.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/imgui_widgets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master//backends/imgui_impl_opengl3.cpp
)

target_link_libraries(gui PRIVATE 
    glfw
    GL
    X11
    pthread
    dl
)

target_include_directories(gui PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/os/include
        ${CMAKE_CURRENT_LIST_DIR}/os/imgui-master # for imgui
)

#   cmake [options] -S <path-to-source> -B <path-to-build>