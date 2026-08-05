# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(audio C CXX)
# Add executable. Default name is the project name, version 0.1

add_executable(${PROJECT_NAME} audio.c
)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    pulse
)


#   cmake [options] -S <path-to-source> -B <path-to-build>