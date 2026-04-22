project(bilog_test VERSION ${PROJECT_VERSION})

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/bilog/src/*_test.cpp
    ${CMAKE_SOURCE_DIR}/preproc/*_test.cpp
    ${CMAKE_SOURCE_DIR}/postproc/*_test.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${CMAKE_SOURCE_DIR}/postproc/schema.cpp
    ${CMAKE_SOURCE_DIR}/preproc/parser.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/preproc
    ${CMAKE_SOURCE_DIR}/postproc
)

find_package(GTest REQUIRED)
find_package(RapidJSON REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE bilog GTest::gtest_main rapidjson)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
