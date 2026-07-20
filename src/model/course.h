#ifndef COURSE_H
#define COURSE_H

#include <string>
#include <vector>

// 表示一个具体的上课时间段，例如周一第 3 节开始，连续 2 节课。
struct TimeSlot
{
    int day = -1;          // 0~6 分别表示 Mon~Sun。
    int beginPeriod = 0;   // 开始节次。
    int duration = 0;      // 持续节数。

    // 返回该时间段的结束节次（包含结束节次）。
    int endPeriod() const;

    // 只判断星期和节次是否重叠，不判断课程周次。
    bool overlaps(const TimeSlot& other) const;
};

// 表示一门基础课程。同一门课程的不同教学班共享这些信息。
struct Course
{
    std::string basicId;                       // course_basic_ID
    std::string name;                          // course_name
    std::string department;                    // department
    std::string semester;                      // 秋季学期 / 春季学期
    int recommendedTerm = 0;                   // recommended_term，建议或最早修读学期
    std::string category;                      // 专业必修课 / 学科基础课 / 专业选修课
    double credit = 0.0;                        // credit
    int beginWeek = 0;                         // beg_week，开始周数
    int durationWeeks = 0;                     // last_week，持续周数
    std::vector<std::string> prerequisiteIds;  // prereq_ID，先修课程 ID 列表

    // 返回课程的最后一周（包含最后一周）。
    int endWeek() const;

    // 判断课程能否安排在第 term 学期。
    bool isAvailableInTerm(int term) const;
};

// 表示一门课程的一个具体教学班。
struct CourseSection
{
    std::string basicId;                 // course_basic_ID
    std::string sectionId;               // course_sp_ID
    std::string teacher;                 // teacher
    std::string classroom;               // classroom
    int capacity = 0;                    // limits，人数上限
    std::vector<TimeSlot> timeSlots;     // 该教学班的全部上课时间段，课程时间是某个课程的教学班的属性而不是某个课程的属性

    // 返回“基础课程 ID#教学班 ID”形式的唯一标识。
    std::string uniqueKey() const;// 课程ID+教学班ID 才可以精准定位到一个具体的教学班
};

#endif // COURSE_H

// Course（基础课程）
//    ├── CourseSection（教学班 1）
//    │      └── TimeSlot（上课时间段，可有多个）
//    ├── CourseSection（教学班 2）
//    │      └── TimeSlot（上课时间段，可有多个）
//    └── CourseSection（教学班 3）
//           └── TimeSlot（上课时间段，可有多个）
