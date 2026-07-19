#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../data/course_repository.h"
#include "../model/manual_course_selection.h"
#include "../model/planning_constraints.h"

#include <array>
#include <string>
#include <vector>

// 排课的输入约束，由调用方（界面或服务层）填充。
struct ScheduleConstraints
{
    // ---------- 现有培养方案约束 ----------

    std::vector<std::string> requiredCourseIds;     // 必须全部安排的课程 ID
    std::vector<std::string> electiveCandidateIds;  // 选修候选池的课程 ID
    std::array<double, 9> maxCreditPerTerm{};       // 每学期学分上限，下标 1~8
    std::array<double, 9> minCreditPerTerm{};       // 每学期学分下限，下标 1~8，0 表示无要求
    double minTotalCredit = 0.0;                    // 八学期总学分下限
    double electiveMinCredit = 0.0;                 // 专业选修课学分下限

    // ---------- 前端传入的个性化时间偏好 ----------

    // 包括培养方案 JSON 中的 avoid_time_blocks 和用户在“时间偏好”页面
    // 选择的时间。hard == false 表示尽量避开（软约束），hard == true
    // 表示严格禁止安排（硬约束）。
    std::vector<TimePreferenceBlock> avoidTimeBlocks;

    // ---------- 用户手动选课 ----------

    // 用户指定的“课程 + 教学班 + 学期”。算法必须优先将这些课程
    // 按指定教学班、指定学期固定安排，不能换班或换学期。
    std::vector<ManualCourseSelection> manualSelections;
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
    std::vector<std::string> problems; // 导致规划失败的原因
    std::vector<std::string> warnings; // 不影响成功判定的提示（例如软时间偏好未完全满足）
};

// 手动选课数据的校验结果。
struct ManualPlanCheckResult
{
    bool valid = true;
    std::vector<std::string> problems;
};

// 贪心 + 拓扑排序的八学期排课器。
class Scheduler
{
public:
    // 校验 constraints.manualSelections 中的手动选课数据：
    // 学期范围、课程与教学班是否存在、能否在指定学期开设、是否重复选择、
    // 手动课程之间是否时间冲突、是否超过学期学分上限、是否违反硬性时间禁用。
    static ManualPlanCheckResult validateManualPlan(
        const CourseRepository& repository,
        const ScheduleConstraints& constraints);

    // 在给定约束下生成一份八学期课表。
    static ScheduleResult makeSchedule(const CourseRepository& repository,
                                       const ScheduleConstraints& constraints);
};

#endif // SCHEDULER_H
