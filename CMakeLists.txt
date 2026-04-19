cmake_minimum_required(VERSION 3.25)

project(bilog VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
set_tests_properties(${test_name} PROPERTIES
    ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/%m.profraw"
)

add_subdirectory(bilog)
add_subdirectory(tests)
add_subdirectory(benches)
add_subdirectory(preproc)
add_subdirectory(postproc)
add_subdirectory(examples)
