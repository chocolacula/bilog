add_library(${PROJECT_NAME}
    src/sink/file.cpp
    src/sink/stdout.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC include)
