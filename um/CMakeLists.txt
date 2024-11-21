cmake_minimum_required(VERSION 3.21)
project(um)

add_executable(um main.cpp 
    "memory_manager.ixx"
    "superfetch.ixx" "driver.ixx")

set_target_properties(um PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/um"
)