#ifndef JSON_READER_H
#define JSON_READER_H// 头文件保护

#include "../model/planning_constraints.h"

#include <string>

// 负责读取专业培养方案 JSON 文件，并转换为 PlanningConstraints。
class JsonReader
{
public:
    // 读取一个专业培养方案。
    // 成功时返回 true，并把结果保存到 constraints。
    // 失败时返回 false，并把失败原因写入 errorMessage。
    static bool loadPlanningConstraints(const std::string& filePath,
                                        PlanningConstraints& constraints,
                                        std::string& errorMessage);
};

#endif // JSON_READER_H
