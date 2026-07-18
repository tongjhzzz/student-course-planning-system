#include "algorithm/scheduler.h"
#include "data/constraints_loader.h"
#include "data/course_repository.h"
#include "data/csv_reader.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

const char* kDayNames[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};

// 把教学班的所有时间段拼成 "周一3-4节 周五12-13节" 的形式。
std::string describeTimeSlots(const CourseSection& section)
{
    std::string text;
    for (const TimeSlot& slot : section.timeSlots) {
        if (!text.empty()) {
            text += " ";
        }
        text += kDayNames[slot.day];
        text += std::to_string(slot.beginPeriod);
        if (slot.duration > 1) {
            text += "-" + std::to_string(slot.endPeriod());
        }
        text += "节";
    }
    return text;
}

void printPlan(const ScheduleResult& result)
{
    for (int term = 1; term <= 8; ++term) {
        std::cout << "----- 第 " << term << " 学期（"
                  << result.creditByTerm[term] << " 学分）-----\n";
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            const Course& course = *planned.course;
            const CourseSection& section = *planned.section;
            std::cout << "  [" << course.category << "] "
                      << course.name << " "
                      << section.uniqueKey() << " "
                      << course.credit << " 学分 "
                      << section.teacher << " "
                      << section.classroom << " "
                      << course.beginWeek << "-" << course.endWeek() << "周 "
                      << describeTimeSlots(section) << '\n';
        }
    }

    std::cout << "----- 汇总 -----\n";
    std::cout << "总学分：" << result.totalCredit << '\n';
    std::cout << "必修+基础学分：" << result.requiredBasicCredit << '\n';
    std::cout << "专业选修学分：" << result.electiveCredit << '\n';
}

// 检查同一学期内是否存在真实时间冲突（周次、星期、节次同时重叠）。
bool checkTimeConflicts(const ScheduleResult& result)
{
    bool ok = true;
    for (int term = 1; term <= 8; ++term) {
        const std::vector<PlannedCourse>& courses = result.coursesByTerm[term];
        for (std::size_t i = 0; i < courses.size(); ++i) {
            for (std::size_t j = i + 1; j < courses.size(); ++j) {
                const Course& a = *courses[i].course;
                const Course& b = *courses[j].course;
                const bool weeksOverlap =
                    a.beginWeek <= b.endWeek() && b.beginWeek <= a.endWeek();
                if (!weeksOverlap) {
                    continue;
                }
                for (const TimeSlot& x : courses[i].section->timeSlots) {
                    for (const TimeSlot& y : courses[j].section->timeSlots) {
                        if (x.overlaps(y)) {
                            std::cout << "  第 " << term << " 学期时间冲突："
                                      << a.name << " 与 " << b.name << '\n';
                            ok = false;
                        }
                    }
                }
            }
        }
    }
    return ok;
}

// 检查先修课是否都排在后继课之前的学期。
bool checkPrerequisiteOrder(const ScheduleResult& result)
{
    std::unordered_map<std::string, int> termById;
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            termById[planned.course->basicId] = term;
        }
    }

    bool ok = true;
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            for (const std::string& prereq : planned.course->prerequisiteIds) {
                auto it = termById.find(prereq);
                if (it == termById.end() || it->second >= term) {
                    std::cout << "  先修顺序错误：" << planned.course->name
                              << "（第 " << term << " 学期）的先修课 "
                              << prereq;
                    if (it == termById.end()) {
                        std::cout << " 未安排\n";
                    } else {
                        std::cout << " 安排在第 " << it->second << " 学期\n";
                    }
                    ok = false;
                }
            }
        }
    }
    return ok;
}

// 检查每学期学分是否都不超过上限。
bool checkCreditCaps(const ScheduleResult& result,
                     const ScheduleConstraints& constraints)
{
    bool ok = true;
    for (int term = 1; term <= 8; ++term) {
        if (result.creditByTerm[term] > constraints.maxCreditPerTerm[term]) {
            std::cout << "  第 " << term << " 学期学分 "
                      << result.creditByTerm[term] << " 超过上限 "
                      << constraints.maxCreditPerTerm[term] << '\n';
            ok = false;
        }
    }
    return ok;
}

// 检查必修课是否全部安排。
bool checkRequiredPlaced(const ScheduleResult& result,
                         const ScheduleConstraints& constraints)
{
    bool ok = true;
    for (const std::string& id : constraints.requiredCourseIds) {
        bool placed = false;
        for (int term = 1; term <= 8 && !placed; ++term) {
            for (const PlannedCourse& planned : result.coursesByTerm[term]) {
                if (planned.course->basicId == id) {
                    placed = true;
                    break;
                }
            }
        }
        if (!placed) {
            std::cout << "  必修课未安排：" << id << '\n';
            ok = false;
        }
    }
    return ok;
}

} // namespace

int main()
{
    CourseRepository repository;
    std::string errorMessage;

    if (!CsvReader::loadCourseInfo("data/course_info.csv", repository, errorMessage)) {
        std::cerr << "读取课程基础信息失败：" << errorMessage << '\n';
        return 1;
    }
    if (!CsvReader::loadCourseTime("data/course_time.csv", repository, errorMessage)) {
        std::cerr << "读取课程上课时间失败：" << errorMessage << '\n';
        return 1;
    }
    std::cout << "课程数据加载成功：基础课程 " << repository.courseCount()
              << " 门，教学班 " << repository.sectionCount() << " 个。\n\n";

    ScheduleConstraints constraints;
    if (!ConstraintsLoader::loadFromJsonFile("data/sample_constraints.json",
                                             constraints, errorMessage)) {
        std::cerr << "读取规划约束失败：" << errorMessage << '\n';
        return 1;
    }
    std::cout << "约束加载成功：必修课 " << constraints.requiredCourseIds.size()
              << " 门，选修候选 " << constraints.electiveCandidateIds.size()
              << " 门，总学分下限 " << constraints.minTotalCredit
              << "，选修学分下限 " << constraints.electiveMinCredit << "。\n\n";

    const ScheduleResult result = Scheduler::makeSchedule(repository, constraints);

    std::cout << "排课是否成功：" << (result.success ? "是" : "否") << "\n\n";
    printPlan(result);

    if (!result.problems.empty()) {
        std::cout << "\n----- 存在的问题 -----\n";
        for (const std::string& problem : result.problems) {
            std::cout << "  " << problem << '\n';
        }
    }

    std::cout << "\n----- 结果校验 -----\n";
    const bool noConflict = checkTimeConflicts(result);
    std::cout << "同学期无时间冲突：" << (noConflict ? "通过" : "未通过") << '\n';
    const bool prereqOk = checkPrerequisiteOrder(result);
    std::cout << "先修顺序正确：" << (prereqOk ? "通过" : "未通过") << '\n';
    const bool capOk = checkCreditCaps(result, constraints);
    std::cout << "每学期学分不超上限：" << (capOk ? "通过" : "未通过") << '\n';
    const bool requiredOk = checkRequiredPlaced(result, constraints);
    std::cout << "必修课全部安排：" << (requiredOk ? "通过" : "未通过") << '\n';
    const bool electiveOk = result.electiveCredit >= constraints.electiveMinCredit;
    std::cout << "选修学分达标：" << (electiveOk ? "通过" : "未通过") << '\n';
    const bool totalOk = result.totalCredit >= constraints.minTotalCredit;
    std::cout << "总学分达标：" << (totalOk ? "通过" : "未通过") << '\n';

    const bool allOk = noConflict && prereqOk && capOk && requiredOk
                       && electiveOk && totalOk;
    return allOk && result.success ? 0 : 1;
}
