project(preproc VERSION ${PROJECT_VERSION})

add_executable(${PROJECT_NAME} main.cpp parser.cpp rewriter.cpp schema.cpp)

find_package(RapidJSON)
find_package(tclap)
target_link_libraries(${PROJECT_NAME} PRIVATE bilog rapidjson tclap::tclap)
target_compile_definitions(${PROJECT_NAME} PRIVATE BILOG_VERSION="${PROJECT_VERSION}")
