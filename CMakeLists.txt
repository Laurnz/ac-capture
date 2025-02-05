cmake_minimum_required(VERSION 3.12)
project(accapture C)

# Ensure we are building for Windows
if(NOT WIN32)
    message(FATAL_ERROR "This project is intended to be built on Windows.")
endif()

# Source files
set(SOURCE_FILES capture.c)

# Include files
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create the library
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Link with the Direct3D 11 library
target_link_libraries(${PROJECT_NAME} d3d11)

# C standard
set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
)

# Install targets
install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib)