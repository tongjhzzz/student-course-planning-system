#ifndef MANUAL_COURSE_SELECTION_H
#define MANUAL_COURSE_SELECTION_H

#include <string>

// 表示用户手动加入方案的一门具体教学班
struct ManualCourseSelection
{
    std::string basicId;   // 课程编号
    std::string sectionId; // 教学班代码
    int term = 0;          // 计划安排的学期，范围为 1 到 8
};

#endif // MANUAL_COURSE_SELECTION_H
