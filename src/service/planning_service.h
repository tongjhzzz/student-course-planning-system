#ifndef PLANNING_SERVICE_H
#define PLANNING_SERVICE_H

#include "../algorithm/scheduler.h"
#include "../data/course_repository.h"
#include "../model/planning_constraints.h"

#include <string>

// 负责把数据读取模块和排课算法模块连接起来。
class PlanningService
{
public:
    // 读取两份课程 CSV 和一份专业培养方案 JSON。
    // repository 必须由调用者保存，因为 ScheduleResult 中的课程指针会指向它。
    static bool loadPlanningData(const std::string& courseInfoPath,
                                 const std::string& courseTimePath,
                                 const std::string& profilePath,
                                 CourseRepository& repository,
                                 PlanningConstraints& constraints,
                                 std::string& errorMessage);

    // 将 JSON 培养方案使用的约束结构转换为排课算法使用的约束结构。
    static ScheduleConstraints toScheduleConstraints(
        const PlanningConstraints& constraints);

    // 根据已加载的数据生成八学期课程规划。
    static ScheduleResult createSchedule(
        const CourseRepository& repository,
        const PlanningConstraints& constraints);
};

#endif // PLANNING_SERVICE_H
