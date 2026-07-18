cmake_minimum_required(VERSION 3.16)

project(IntelligentCoursePlanner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(IntelligentCoursePlanner
    src/main.cpp
    src/model/course.cpp
    src/data/course_repository.cpp
    src/data/csv_reader.cpp
    src/data/json_reader.cpp
)

target_link_libraries(IntelligentCoursePlanner PRIVATE Qt6::Core)
