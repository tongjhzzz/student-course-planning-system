#ifndef CONSTRAINTS_LOADER_H
#define CONSTRAINTS_LOADER_H

#include "../algorithm/scheduler.h"

#include <string>

// 负责把 JSON 约束文件（如 data/sample_constraints.json）读取为
// Scheduler 需要的 ScheduleConstraints。算法模块本身不做文件读写。
class ConstraintsLoader
{
public:
    // 读取约束文件并填充 constraints。
    // 需要的字段：
    //   target_course_scope.required_course_basic_IDs
    //   target_course_scope.elective_candidate_course_basic_IDs
    //   max_credit_per_semester（"1"~"8"）
    //   min_total_credit
    //   elective_min_credit
    // 失败时返回 false，并把原因写入 errorMessage。
    static bool loadFromJsonFile(const std::string& filePath,
                                 ScheduleConstraints& constraints,
                                 std::string& errorMessage);
};

#endif // CONSTRAINTS_LOADER_H
