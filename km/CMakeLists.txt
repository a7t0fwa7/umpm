cmake_minimum_required(VERSION 3.21)
project(km)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
find_package(WDK REQUIRED)

wdk_add_driver(km
    main.cpp
)

target_include_directories(km PRIVATE
    ${CMAKE_SOURCE_DIR}
)

set_target_properties(km PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/km"
)