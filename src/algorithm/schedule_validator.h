#ifndef SCHEDULE_VALIDATOR_H
#define SCHEDULE_VALIDATOR_H

#include "scheduler.h"

#include <string>
#include <vector>

// 一份已有课表的检查结果。problems 为空表示完全合法。
struct ValidationResult
{
    bool ok = false;
    std::vector<std::string> problems;
};

// 独立的手动选课验证器。
// 与 Scheduler::makeSchedule（从零生成规划）不同，本类只检查一份
// 已经存在的课表是否合法，供手动选课、取消选课后重新检查使用。
class ScheduleValidator
{
public:
    // 检查 schedule 是否满足 constraints 的全部约束：
    // 课程与教学班存在、开课学期匹配、同学期无时间冲突、
    // 先修顺序正确、每学期学分上下限、必修课全部安排、选修/总学分达标。
    // 每学期学分从 coursesByTerm 重新计算，不信任调用方填入的 creditByTerm。
    static ValidationResult validate(const CourseRepository& repository,
                                     const ScheduleConstraints& constraints,
                                     const ScheduleResult& schedule);
};

#endif // SCHEDULE_VALIDATOR_H
