#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../data/course_repository.h"

#include <array>
#include <string>
#include <vector>

// 排课的输入约束，由调用方（界面或服务层）填充。
struct ScheduleConstraints
{
    std::vector<std::string> requiredCourseIds;     // 必须全部安排的课程 ID
    std::vector<std::string> electiveCandidateIds;  // 选修候选池的课程 ID
    std::array<double, 9> maxCreditPerTerm{};       // 每学期学分上限，下标 1~8
    std::array<double, 9> minCreditPerTerm{};       // 每学期学分下限，下标 1~8，0 表示无要求
    double minTotalCredit = 0.0;                    // 八学期总学分下限
    double electiveMinCredit = 0.0;                 // 专业选修课学分下限
};

// 一门已经排入学期的课程。
struct PlannedCourse
{
    const Course* course = nullptr;       // 基础课程
    const CourseSection* section = nullptr; // 选中的教学班
    int term = 0;                         // 安排到的学期，1~8
};

// 排课结果。problems 保存无法安排或学分不足等原因，供界面解释。
struct ScheduleResult
{
    bool success = false;
    std::array<std::vector<PlannedCourse>, 9> coursesByTerm; // 每学期课表，下标 1~8
    std::array<double, 9> creditByTerm{};                    // 每学期学分
    double totalCredit = 0.0;         // 总学分
    double requiredBasicCredit = 0.0; // 专业必修课 + 学科基础课学分
    double electiveCredit = 0.0;      // 专业选修课学分
    std::vector<std::string> problems; // 失败或警告原因
};

// 贪心 + 拓扑排序的八学期排课器。
class Scheduler
{
public:
    // 在给定约束下生成一份八学期课表。
    static ScheduleResult makeSchedule(const CourseRepository& repository,
                                       const ScheduleConstraints& constraints);
};

#endif // SCHEDULER_H
