cmake_minimum_required(VERSION 3.25)
project(bilog_integration CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED BILOG_ROOT)
  message(FATAL_ERROR "BILOG_ROOT must be set to the bilog repository root")
endif()

add_subdirectory(${BILOG_ROOT}/bilog ${CMAKE_BINARY_DIR}/bilog)

add_executable(app main.cpp)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app PRIVATE bilog)
