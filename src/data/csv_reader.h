#ifndef CSV_READER_H
#define CSV_READER_H

#include "course_repository.h"

#include <string>
#include <unordered_map>
#include <vector>

// 负责读取课程 CSV 文件，并把读取结果保存到 CourseRepository 中。
class CsvReader
{
public:
    // 读取 course_info.csv，创建基础课程和教学班。
    // 失败时返回 false，并把原因写入 errorMessage。
    static bool loadCourseInfo(const std::string& filePath,
                               CourseRepository& repository,
                               std::string& errorMessage);

    // 读取 course_time.csv，为已有教学班补充上课时间。
    // 调用前应先成功读取 course_info.csv。
    static bool loadCourseTime(const std::string& filePath,
                               CourseRepository& repository,
                               std::string& errorMessage);

private:
    // 将一行 CSV 按逗号拆分为多个字段，支持双引号中的逗号。
    static std::vector<std::string> parseCsvLine(const std::string& line);

    // 去除字符串两端的空格、制表符和换行符。
    static std::string trim(const std::string& text);

    // 将 "A;B;C" 拆分为 {"A", "B", "C"}。
    static std::vector<std::string> splitPrerequisiteIds(
        const std::string& prerequisiteText);

    // 将 Mon、Tue ... Sun 转换为 0~6；无法识别时返回 -1。
    static int dayToNumber(const std::string& dayText);

    // 建立“列名 -> 列号”的对应关系，并检查必须字段是否存在。
    static bool buildHeaderIndex(
        const std::vector<std::string>& header,
        const std::vector<std::string>& requiredColumns,
        std::unordered_map<std::string, int>& headerIndex,
        std::string& errorMessage);

    // 安全取得某一列的文本；列不存在或行数据不完整时返回空字符串。
    static std::string getField(const std::vector<std::string>& fields,
                                const std::unordered_map<std::string, int>& headerIndex,
                                const std::string& columnName);
};

#endif // CSV_READER_H

// course_info.csv
//     ↓
// 创建 Course（基础课程）
// 创建 CourseSection（教学班）
//     ↓
// course_time.csv
//     ↓
// 找到对应的 CourseSection
// 为它添加 TimeSlot（上课时间）