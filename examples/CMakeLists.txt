project(example VERSION ${PROJECT_VERSION})

add_executable(stdout stdout.cpp)
target_link_libraries(stdout PRIVATE bilog)

add_executable(binary_file binary_file.cpp)
target_link_libraries(binary_file PRIVATE bilog)
