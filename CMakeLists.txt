cmake_minimum_required(VERSION 3.25)

project(bilog_workspace VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(bilog)
add_subdirectory(tests)
add_subdirectory(benches)
add_subdirectory(preproc)
add_subdirectory(postproc)
add_subdirectory(examples)
