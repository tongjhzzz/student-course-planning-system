#ifndef TIME_PREFERENCE_STORAGE_H
#define TIME_PREFERENCE_STORAGE_H

#include "../model/planning_constraints.h"

#include <string>
#include <vector>

class QString;

// 负责保存和读取用户设置的“尽量避开上课时间”。
class TimePreferenceStorage
{
public:
    // 读取时间偏好文件。文件尚不存在时，按“没有设置偏好”处理。
    static bool loadAvoidTimeBlocks(
        const std::string& filePath,
        std::vector<TimePreferenceBlock>& avoidTimeBlocks,
        std::string& errorMessage);

    // 将时间偏好写入 JSON 文件。
    static bool saveAvoidTimeBlocks(
        const std::string& filePath,
        const std::vector<TimePreferenceBlock>& avoidTimeBlocks,
        std::string& errorMessage);

private:
    static std::string dayToText(int day);
    static int textToDay(const QString& dayText);
};

#endif // TIME_PREFERENCE_STORAGE_H
