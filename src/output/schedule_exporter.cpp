#include "schedule_exporter.h"

#include <fstream>

std::string ScheduleExporter::escapeCsvField(const std::string& text)
{
    bool needsQuotes = false;
    std::string escapedText;

    for (const char character : text) {
        if (character == '"') {
            escapedText += "\"\"";
            needsQuotes = true;
        } else {
            escapedText += character;

            if (character == ',' || character == '\n' || character == '\r') {
                needsQuotes = true;
            }
        }
    }

    if (needsQuotes) {
        return "\"" + escapedText + "\"";
    }

    return escapedText;
}

std::string ScheduleExporter::weekdayName(int day)
{
    const char* const weekdayNames[] = {
        "周一", "周二", "周三", "周四", "周五", "周六", "周日"
    };

    if (day < 0 || day > 6) {
        return "";
    }

    return weekdayNames[day];
}

bool ScheduleExporter::exportToCsv(const ScheduleResult& result,
                                    const std::string& filePath,
                                    std::string& errorMessage)
{
    std::ofstream outputFile(filePath);
    if (!outputFile.is_open()) {
        errorMessage = "无法创建导出文件：" + filePath;
        return false;
    }

    // UTF-8 BOM 使 Excel 打开 CSV 时能正确识别中文。
    outputFile << "\xEF\xBB\xBF";
    outputFile << "学期,课程编号,课程名称,教学班代码,课程类别,学分,开课院系,"
                  "开课季节,教师,教室,开始周,结束周,星期,开始节次,结束节次\n";

    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            if (planned.course == nullptr || planned.section == nullptr) {
                continue;
            }

            const Course& course = *planned.course;
            const CourseSection& section = *planned.section;

            // 一门课可能在不同时间段上课，因此每个时间段输出一行。
            for (const TimeSlot& slot : section.timeSlots) {
                outputFile
                    << term << ','
                    << escapeCsvField(course.basicId) << ','
                    << escapeCsvField(course.name) << ','
                    << escapeCsvField(section.sectionId) << ','
                    << escapeCsvField(course.category) << ','
                    << course.credit << ','
                    << escapeCsvField(course.department) << ','
                    << escapeCsvField(course.semester) << ','
                    << escapeCsvField(section.teacher) << ','
                    << escapeCsvField(section.classroom) << ','
                    << course.beginWeek << ','
                    << course.endWeek() << ','
                    << escapeCsvField(weekdayName(slot.day)) << ','
                    << slot.beginPeriod << ','
                    << slot.endPeriod() << '\n';
            }
        }
    }

    if (!outputFile.good()) {
        errorMessage = "写入导出文件时发生错误：" + filePath;
        return false;
    }

    errorMessage.clear();
    return true;
}
