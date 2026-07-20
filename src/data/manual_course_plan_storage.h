#ifndef MANUAL_COURSE_PLAN_STORAGE_H
#define MANUAL_COURSE_PLAN_STORAGE_H

#include "../model/manual_course_selection.h"

#include <string>
#include <vector>

// 负责保存和读取用户手动选择的课程方案
class ManualCoursePlanStorage
{
public:
    // 读取已保存的手动选课方案。文件不存在时按空方案处理。
    static bool loadSelections(
        const std::string& filePath,
        std::vector<ManualCourseSelection>& selections,
        std::string& errorMessage);

    // 将当前手动选课方案写入 JSON 文件。
    static bool saveSelections(
        const std::string& filePath,
        const std::vector<ManualCourseSelection>& selections,
        std::string& errorMessage);
};

#endif // MANUAL_COURSE_PLAN_STORAGE_H
