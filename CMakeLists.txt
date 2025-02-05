cmake_minimum_required(VERSION 3.10)
project(D3D11TextureCapture C)

# Ensure we are building for Windows
if(NOT WIN32)
    message(FATAL_ERROR "This project is intended to be built on Windows.")
endif()

# Specify the source file (adjust the name if different)
set(SOURCE_FILES main.c)

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Link with the Direct3D 11 library
target_link_libraries(${PROJECT_NAME} d3d11)

# Optionally, you can set the C standard (if required)
set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
)
