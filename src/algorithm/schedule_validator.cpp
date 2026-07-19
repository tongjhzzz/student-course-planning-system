#include "schedule_validator.h"

#include <array>
#include <unordered_map>

namespace {

// 判断两门课程的上课周次是否有交集。
bool weeksOverlap(const Course& a, const Course& b)
{
    return a.beginWeek <= b.endWeek() && b.beginWeek <= a.endWeek();
}

} // namespace

ValidationResult ScheduleValidator::validate(
    const CourseRepository& repository,
    const ScheduleConstraints& constraints,
    const ScheduleResult& schedule)
{
    ValidationResult validation;

    // 每学期学分从课表重新计算，并记录每门课所在学期。
    std::array<double, 9> creditByTerm{};
    std::unordered_map<std::string, int> termById;

    for (int term = 1; term <= 8; ++term) {
        const std::vector<PlannedCourse>& courses = schedule.coursesByTerm[term];

        for (const PlannedCourse& planned : courses) {
            const Course& course = *planned.course;

            // 课程与教学班必须真实存在，且教学班属于该课程。
            const CourseSection* knownSection =
                repository.findSection(course.basicId, planned.section->sectionId);
            if (repository.findCourse(course.basicId) == nullptr
                || knownSection == nullptr) {
                validation.problems.push_back(
                    "课程 " + course.basicId + " " + course.name
                    + " 或所选教学班在课程数据中不存在。");
                continue;
            }

            if (termById.count(course.basicId) != 0) {
                validation.problems.push_back(
                    "课程 " + course.basicId + " " + course.name + " 被重复安排。");
            } else {
                termById[course.basicId] = term;
            }

            if (!course.isAvailableInTerm(term)) {
                validation.problems.push_back(
                    "课程 " + course.basicId + " " + course.name + " 第 "
                    + std::to_string(term) + " 学期不开课。");
            }

            creditByTerm[term] += course.credit;
        }

        // 同学期时间冲突：周次、星期、节次同时重叠才算冲突。
        for (std::size_t i = 0; i < courses.size(); ++i) {
            for (std::size_t j = i + 1; j < courses.size(); ++j) {
                const Course& a = *courses[i].course;
                const Course& b = *courses[j].course;
                if (!weeksOverlap(a, b)) {
                    continue;
                }
                for (const TimeSlot& x : courses[i].section->timeSlots) {
                    for (const TimeSlot& y : courses[j].section->timeSlots) {
                        if (x.overlaps(y)) {
                            validation.problems.push_back(
                                "第 " + std::to_string(term) + " 学期时间冲突："
                                + a.name + " 与 " + b.name + "。");
                        }
                    }
                }
            }
        }

        // 每学期学分上限与下限。
        if (creditByTerm[term] > constraints.maxCreditPerTerm[term]) {
            validation.problems.push_back(
                "第 " + std::to_string(term) + " 学期学分超过上限：当前 "
                + std::to_string(creditByTerm[term]) + " 学分，上限 "
                + std::to_string(constraints.maxCreditPerTerm[term]) + " 学分。");
        }
        if (creditByTerm[term] < constraints.minCreditPerTerm[term]) {
            validation.problems.push_back(
                "第 " + std::to_string(term) + " 学期学分不足：当前 "
                + std::to_string(creditByTerm[term]) + " 学分，要求至少 "
                + std::to_string(constraints.minCreditPerTerm[term]) + " 学分。");
        }
    }

    // 先修顺序：先修课必须排在更早学期。
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : schedule.coursesByTerm[term]) {
            const Course& course = *planned.course;
            for (const std::string& prereq : course.prerequisiteIds) {
                auto it = termById.find(prereq);
                if (it == termById.end()) {
                    validation.problems.push_back(
                        "课程 " + course.name + "（第 " + std::to_string(term)
                        + " 学期）的先修课 " + prereq + " 未安排。");
                } else if (it->second >= term) {
                    validation.problems.push_back(
                        "课程 " + course.name + "（第 " + std::to_string(term)
                        + " 学期）的先修课 " + prereq + " 安排在第 "
                        + std::to_string(it->second) + " 学期，顺序错误。");
                }
            }
        }
    }

    // 必修课必须全部安排。
    for (const std::string& id : constraints.requiredCourseIds) {
        if (termById.count(id) == 0) {
            validation.problems.push_back("必修课 " + id + " 未安排。");
        }
    }

    // 总学分与选修学分。
    double totalCredit = 0.0;
    double electiveCredit = 0.0;
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : schedule.coursesByTerm[term]) {
            totalCredit += planned.course->credit;
            if (planned.course->category == "专业选修课") {
                electiveCredit += planned.course->credit;
            }
        }
    }
    if (totalCredit < constraints.minTotalCredit) {
        validation.problems.push_back(
            "总学分不足：当前 " + std::to_string(totalCredit)
            + " 学分，要求至少 " + std::to_string(constraints.minTotalCredit)
            + " 学分。");
    }
    if (electiveCredit < constraints.electiveMinCredit) {
        validation.problems.push_back(
            "专业选修课学分不足：当前 " + std::to_string(electiveCredit)
            + " 学分，要求至少 " + std::to_string(constraints.electiveMinCredit)
            + " 学分。");
    }

    validation.ok = validation.problems.empty();
    return validation;
}
