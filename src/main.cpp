#include "data/course_repository.h"
#include "data/csv_reader.h"
#include "data/json_reader.h"

#include <iostream>
#include <string>

int main()
{
    CourseRepository repository;
    std::string errorMessage;

    const std::string courseInfoPath = "data/course_info.csv";
    const std::string courseTimePath = "data/course_time.csv";
    const std::string profilePath =
        "data/major_profiles/computer_science.json";

    if (!CsvReader::loadCourseInfo(courseInfoPath, repository, errorMessage)) {
        std::cerr << "读取课程基础信息失败：" << errorMessage << '\n';
        return 1;
    }

    if (!CsvReader::loadCourseTime(courseTimePath, repository, errorMessage)) {
        std::cerr << "读取课程上课时间失败：" << errorMessage << '\n';
        return 1;
    }

    PlanningConstraints constraints;

    if (!JsonReader::loadPlanningConstraints(profilePath, constraints, errorMessage)) {
        std::cerr << "读取专业培养方案失败：" << errorMessage << '\n';
        return 1;
    }

    std::cout << "课程数据加载成功。\n";
    std::cout << "基础课程数量：" << repository.courseCount() << '\n';
    std::cout << "教学班数量：" << repository.sectionCount() << '\n';
    std::cout << "培养方案名称：" << constraints.profileName << '\n';
    std::cout << "必修课数量：" << constraints.requiredCourseIds.size() << '\n';
    std::cout << "候选选修课数量："
              << constraints.electiveCandidateCourseIds.size() << '\n';
    std::cout << "毕业最低总学分：" << constraints.minTotalCredit << '\n';

    return 0;
}
