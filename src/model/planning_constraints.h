#ifndef PLANNING_CONSTRAINTS_H
#define PLANNING_CONSTRAINTS_H

#include <array>
#include <string>
#include <vector>

// 表示培养方案中“尽量避开”或“优先安排”的一个时间段。
struct TimePreferenceBlock// 时间偏好
{
    int day = -1;          // 星期一到星期日分别用 0 到 6 表示。
    int beginPeriod = 0;   // 开始节次。
    int duration = 0;      // 持续节数。
    bool hard = false;     // true 表示严格禁止，false 表示仅作为偏好。
    std::string reason;    // 设置该时间段的原因。
};

// 保存一个专业培养方案中与课程规划有关的约束条件。
struct PlanningConstraints
{
    std::string profileId;// 培养方案ID
    std::string profileName;// 培养方案名字
    std::string description;// 培养方案描述
    std::string targetDepartment;// 培养方案面向的专业
    std::vector<std::string> supportDepartments;// 支持或适用的其他院系列表

    // 必修课程和候选选修课程的基础课程编号。
    std::vector<std::string> requiredCourseIds;
    std::vector<std::string> electiveCandidateCourseIds;

    // 数组下标 0~7 分别表示第 1~8 学期的最低、最高学分要求。
    std::array<double, 8> minCreditPerSemester{};
    std::array<double, 8> maxCreditPerSemester{};

    double minTotalCredit = 0.0;
    double requiredCredit = 0.0;
    double electiveMinCredit = 0.0;

    std::vector<TimePreferenceBlock> avoidTimeBlocks;
    std::vector<TimePreferenceBlock> preferredTimeBlocks;
};

#endif // PLANNING_CONSTRAINTS_H
