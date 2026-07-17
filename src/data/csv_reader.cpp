#include "csv_reader.h"

#include <fstream>
#include <sstream>

bool CsvReader::loadCourseInfo(const std::string& filePath,
                               CourseRepository& repository,
                               std::string& errorMessage)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        errorMessage = "无法打开课程信息文件：" + filePath;
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        errorMessage = "课程信息文件为空：" + filePath;
        return false;
    }

    std::vector<std::string> header = parseCsvLine(line);
    std::unordered_map<std::string, int> headerIndex;
    const std::vector<std::string> requiredColumns = {
        "course_basic_ID", "course_sp_ID", "course_name", "department",
        "semester", "recommended_term", "category", "credit", "beg_week",
        "last_week", "teacher", "classroom", "limits", "prereq_ID"
    };

    if (!buildHeaderIndex(header, requiredColumns, headerIndex, errorMessage)) {
        return false;
    }

    int lineNumber = 1;

    while (std::getline(file, line)) {
        ++lineNumber;

        if (trim(line).empty()) {
            continue;
        }

        std::vector<std::string> fields = parseCsvLine(line);

        try {
            const std::string basicId = getField(fields, headerIndex, "course_basic_ID");
            const std::string sectionId = getField(fields, headerIndex, "course_sp_ID");

            if (basicId.empty() || sectionId.empty()) {
                errorMessage = "课程信息文件第 " + std::to_string(lineNumber)
                               + " 行缺少课程 ID 或教学班 ID。";
                return false;
            }

            // 同一门基础课程只创建一次；不同教学班只保存各自不同的信息。
            if (repository.findCourse(basicId) == nullptr) {
                Course course;
                course.basicId = basicId;
                course.name = getField(fields, headerIndex, "course_name");
                course.department = getField(fields, headerIndex, "department");
                course.semester = getField(fields, headerIndex, "semester");
                course.recommendedTerm = std::stoi(
                    getField(fields, headerIndex, "recommended_term"));
                course.category = getField(fields, headerIndex, "category");
                course.credit = std::stod(getField(fields, headerIndex, "credit"));
                course.beginWeek = std::stoi(getField(fields, headerIndex, "beg_week"));
                course.durationWeeks = std::stoi(
                    getField(fields, headerIndex, "last_week"));
                course.prerequisiteIds = splitPrerequisiteIds(
                    getField(fields, headerIndex, "prereq_ID"));

                if (!repository.addCourse(course)) {
                    errorMessage = "无法添加课程 " + basicId + "。";
                    return false;
                }
            }

            CourseSection section;
            section.basicId = basicId;
            section.sectionId = sectionId;
            section.teacher = getField(fields, headerIndex, "teacher");
            section.classroom = getField(fields, headerIndex, "classroom");
            section.capacity = std::stoi(getField(fields, headerIndex, "limits"));

            if (!repository.addSection(section)) {
                errorMessage = "课程信息文件第 " + std::to_string(lineNumber)
                               + " 行的教学班重复或数据不合法："
                               + section.uniqueKey();
                return false;
            }
        } catch (const std::exception&) {
            errorMessage = "课程信息文件第 " + std::to_string(lineNumber)
                           + " 行的数字字段格式不正确。";
            return false;
        }
    }

    return true;
}

bool CsvReader::loadCourseTime(const std::string& filePath,
                               CourseRepository& repository,
                               std::string& errorMessage)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        errorMessage = "无法打开课程时间文件：" + filePath;
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        errorMessage = "课程时间文件为空：" + filePath;
        return false;
    }

    std::vector<std::string> header = parseCsvLine(line);
    std::unordered_map<std::string, int> headerIndex;
    const std::vector<std::string> requiredColumns = {
        "course_basic_ID", "course_sp_ID", "day", "beg", "last"
    };

    if (!buildHeaderIndex(header, requiredColumns, headerIndex, errorMessage)) {
        return false;
    }

    int lineNumber = 1;

    while (std::getline(file, line)) {
        ++lineNumber;

        if (trim(line).empty()) {
            continue;
        }

        std::vector<std::string> fields = parseCsvLine(line);

        try {
            const std::string basicId = getField(fields, headerIndex, "course_basic_ID");
            const std::string sectionId = getField(fields, headerIndex, "course_sp_ID");
            const int day = dayToNumber(getField(fields, headerIndex, "day"));

            if (basicId.empty() || sectionId.empty() || day == -1) {
                errorMessage = "课程时间文件第 " + std::to_string(lineNumber)
                               + " 行的课程 ID、教学班 ID 或星期不合法。";
                return false;
            }

            TimeSlot timeSlot;
            timeSlot.day = day;
            timeSlot.beginPeriod = std::stoi(getField(fields, headerIndex, "beg"));
            timeSlot.duration = std::stoi(getField(fields, headerIndex, "last"));

            if (!repository.addTimeSlot(basicId, sectionId, timeSlot)) {
                errorMessage = "课程时间文件第 " + std::to_string(lineNumber)
                               + " 行找不到对应教学班或时间不合法："
                               + basicId + "#" + sectionId;
                return false;
            }
        } catch (const std::exception&) {
            errorMessage = "课程时间文件第 " + std::to_string(lineNumber)
                           + " 行的节次格式不正确。";
            return false;
        }
    }

    return true;
}

std::vector<std::string> CsvReader::parseCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string currentField;
    bool insideQuotes = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        char character = line[index];

        if (character == '"') {
            // CSV 中连续两个双引号表示字段内容中的一个双引号。
            if (insideQuotes && index + 1 < line.size() && line[index + 1] == '"') {
                currentField += '"';
                ++index;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (character == ',' && !insideQuotes) {
            fields.push_back(trim(currentField));
            currentField.clear();
        } else {
            currentField += character;
        }
    }

    fields.push_back(trim(currentField));
    return fields;
}

std::string CsvReader::trim(const std::string& text)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);

    if (first == std::string::npos) {
        return "";
    }

    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::vector<std::string> CsvReader::splitPrerequisiteIds(
    const std::string& prerequisiteText)
{
    std::vector<std::string> prerequisiteIds;
    std::stringstream stream(prerequisiteText);
    std::string id;

    while (std::getline(stream, id, ';')) {
        id = trim(id);

        if (!id.empty()) {
            prerequisiteIds.push_back(id);
        }
    }

    return prerequisiteIds;
}

int CsvReader::dayToNumber(const std::string& dayText)
{
    if (dayText == "Mon") return 0;
    if (dayText == "Tue") return 1;
    if (dayText == "Wed") return 2;
    if (dayText == "Thu") return 3;
    if (dayText == "Fri") return 4;
    if (dayText == "Sat") return 5;
    if (dayText == "Sun") return 6;

    return -1;
}

bool CsvReader::buildHeaderIndex(
    const std::vector<std::string>& header,
    const std::vector<std::string>& requiredColumns,
    std::unordered_map<std::string, int>& headerIndex,
    std::string& errorMessage)
{
    headerIndex.clear();

    for (std::size_t index = 0; index < header.size(); ++index) {
        std::string columnName = trim(header[index]);

        // UTF-8 BOM 可能出现在 CSV 文件第一个表头字段的开头。
        if (index == 0 && columnName.size() >= 3
            && static_cast<unsigned char>(columnName[0]) == 0xEF
            && static_cast<unsigned char>(columnName[1]) == 0xBB
            && static_cast<unsigned char>(columnName[2]) == 0xBF) {
            columnName.erase(0, 3);
        }

        headerIndex[columnName] = static_cast<int>(index);
    }

    for (const std::string& columnName : requiredColumns) {
        if (headerIndex.find(columnName) == headerIndex.end()) {
            errorMessage = "CSV 文件缺少必要字段：" + columnName;
            return false;
        }
    }

    return true;
}

std::string CsvReader::getField(
    const std::vector<std::string>& fields,
    const std::unordered_map<std::string, int>& headerIndex,
    const std::string& columnName)
{
    auto indexIt = headerIndex.find(columnName);

    if (indexIt == headerIndex.end()) {
        return "";
    }

    int index = indexIt->second;

    if (index < 0 || static_cast<std::size_t>(index) >= fields.size()) {
        return "";
    }

    return trim(fields[static_cast<std::size_t>(index)]);
}
