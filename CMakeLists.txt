cmake_minimum_required(VERSION 3.16)

project(IntelligentCoursePlanner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(IntelligentCoursePlanner
    src/main.cpp
    src/model/course.cpp
    src/data/course_repository.cpp
    src/data/csv_reader.cpp
    src/data/constraints_loader.cpp
    src/algorithm/scheduler.cpp
)

# Windows + MinGW 下静态链接 libstdc++/libgcc，
# 避免 exe 运行时加载到 PATH 中其他工具链的 DLL 而崩溃。
if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(IntelligentCoursePlanner PRIVATE -static)
endif()
