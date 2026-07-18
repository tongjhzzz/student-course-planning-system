#include "data/course_repository.h"
#include "data/csv_reader.h"

#include <iostream>
#include <string>

int main()
{
    CourseRepository repository;
    std::string errorMessage;

    const std::string courseInfoPath = "data/course_info.csv";
    const std::string courseTimePath = "data/course_time.csv";

    if (!CsvReader::loadCourseInfo(courseInfoPath, repository, errorMessage)) {
        std::cerr << "读取课程基础信息失败：" << errorMessage << '\n';
        return 1;
    }

    if (!CsvReader::loadCourseTime(courseTimePath, repository, errorMessage)) {
        std::cerr << "读取课程上课时间失败：" << errorMessage << '\n';
        return 1;
    }

    std::cout << "课程数据加载成功。\n";
    std::cout << "基础课程数量：" << repository.courseCount() << '\n';
    std::cout << "教学班数量：" << repository.sectionCount() << '\n';

    return 0;
}
