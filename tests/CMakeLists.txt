project(bilog_test VERSION ${PROJECT_VERSION})

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/bilog/src/*_test.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

find_package(GTest REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE bilog GTest::gtest_main)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
