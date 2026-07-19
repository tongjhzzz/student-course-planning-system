#ifndef SCHEDULE_EXPORTER_H
#define SCHEDULE_EXPORTER_H

#include "../algorithm/scheduler.h"

#include <string>

// 将已经生成的八学期课程规划写入 CSV 文件。
class ScheduleExporter
{
public:
    // 成功返回 true；失败时返回 false，并在 errorMessage 中说明原因。
    static bool exportToCsv(const ScheduleResult& result,
                            const std::string& filePath,
                            std::string& errorMessage);

private:
    // CSV 字段中含有逗号、换行或双引号时，需要转义。
    static std::string escapeCsvField(const std::string& text);
    static std::string weekdayName(int day);
};

#endif // SCHEDULE_EXPORTER_H
