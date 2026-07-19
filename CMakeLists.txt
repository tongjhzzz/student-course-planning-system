cmake_minimum_required(VERSION 3.16)

project(IntelligentCoursePlanner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(IntelligentCoursePlanner
    src/main.cpp
    src/model/course.cpp
    src/data/course_repository.cpp
    src/data/csv_reader.cpp
    src/data/json_reader.cpp
    src/data/time_preference_storage.cpp
    src/data/manual_course_plan_storage.cpp
    src/algorithm/scheduler.cpp
    src/service/planning_service.cpp
    src/output/schedule_exporter.cpp
    src/ui/main_window.cpp
)

target_compile_definitions(IntelligentCoursePlanner PRIVATE
    PROJECT_SOURCE_DIRECTORY="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(IntelligentCoursePlanner PRIVATE Qt6::Core Qt6::Widgets)

# 排课算法单元测试（控制台程序，不依赖 Qt 界面）。
add_executable(scheduler_tests
    tests/scheduler_tests.cpp
    src/model/course.cpp
    src/data/course_repository.cpp
    src/data/csv_reader.cpp
    src/data/json_reader.cpp
    src/algorithm/scheduler.cpp
    src/service/planning_service.cpp
)

target_link_libraries(scheduler_tests PRIVATE Qt6::Core)
