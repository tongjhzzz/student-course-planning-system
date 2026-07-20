cmake_minimum_required(VERSION 3.16)# cmake版本

project(IntelligentCoursePlanner LANGUAGES CXX)# 设置项目名称

set(CMAKE_CXX_STANDARD 17)# 使用c++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)# 调用了项目外的包，Qt需要Core和Widgets这连个部分

# 要生成名为 IntelligentCoursePlanner 的可执行程序时，需要把这里列出的所有 .cpp 源文件都一起编译，再把编译后的结果链接成同一个程序
add_executable(IntelligentCoursePlanner# 使用指定的原文件来向项目中添加可执行文件,生成IntelligentCoursePlanner可执行文件，用来运行Qt图形界面的智能课程规划系统
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
add_executable(scheduler_tests# 生成scheduler_tests可执行文件，用来自动测试排课算法是否符合规则
    tests/scheduler_tests.cpp
    src/model/course.cpp
    src/data/course_repository.cpp
    src/data/csv_reader.cpp
    src/data/json_reader.cpp
    src/algorithm/scheduler.cpp
    src/service/planning_service.cpp
)

target_link_libraries(scheduler_tests PRIVATE Qt6::Core)
