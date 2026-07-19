#include "planning_service.h"

#include "../data/csv_reader.h"
#include "../data/json_reader.h"

bool PlanningService::loadPlanningData(const std::string& courseInfoPath,
                                       const std::string& courseTimePath,
                                       const std::string& profilePath,
                                       CourseRepository& repository,
                                       PlanningConstraints& constraints,
                                       std::string& errorMessage)
{
    repository.clear();

    if (!CsvReader::loadCourseInfo(courseInfoPath, repository, errorMessage)) {
        return false;
    }

    if (!CsvReader::loadCourseTime(courseTimePath, repository, errorMessage)) {
        return false;
    }

    if (!JsonReader::loadPlanningConstraints(profilePath,
                                              constraints,
                                              errorMessage)) {
        return false;
    }

    errorMessage.clear();
    return true;
}

ScheduleConstraints PlanningService::toScheduleConstraints(
    const PlanningConstraints& constraints)
{
    return toScheduleConstraints(constraints, {}, {});
}

ScheduleConstraints PlanningService::toScheduleConstraints(
    const PlanningConstraints& profileConstraints,
    const std::vector<TimePreferenceBlock>& userAvoidTimeBlocks,
    const std::vector<ManualCourseSelection>& manualSelections)
{
    ScheduleConstraints scheduleConstraints;
    scheduleConstraints.requiredCourseIds =
        profileConstraints.requiredCourseIds;
    scheduleConstraints.electiveCandidateIds =
        profileConstraints.electiveCandidateCourseIds;
    scheduleConstraints.minTotalCredit = profileConstraints.minTotalCredit;
    scheduleConstraints.electiveMinCredit = profileConstraints.electiveMinCredit;

    // PlanningConstraints 的下标 0~7 对应第 1~8 学期；
    // ScheduleConstraints 的下标 1~8 对应第 1~8 学期。
    for (int term = 1; term <= 8; ++term) {
        scheduleConstraints.minCreditPerTerm[term] =
            profileConstraints.minCreditPerSemester[term - 1];
        scheduleConstraints.maxCreditPerTerm[term] =
            profileConstraints.maxCreditPerSemester[term - 1];
    }

    // 先加入培养方案自身的时间偏好，
    // 再合并用户在界面中选择的时间偏好。
    scheduleConstraints.avoidTimeBlocks = profileConstraints.avoidTimeBlocks;
    scheduleConstraints.avoidTimeBlocks.insert(
        scheduleConstraints.avoidTimeBlocks.end(),
        userAvoidTimeBlocks.begin(),
        userAvoidTimeBlocks.end());

    // 传入用户手动选课。
    scheduleConstraints.manualSelections = manualSelections;

    return scheduleConstraints;
}

ScheduleResult PlanningService::createSchedule(
    const CourseRepository& repository,
    const PlanningConstraints& constraints)
{
    return createSchedule(repository, constraints, {}, {});
}

ScheduleResult PlanningService::createSchedule(
    const CourseRepository& repository,
    const PlanningConstraints& profileConstraints,
    const std::vector<TimePreferenceBlock>& userAvoidTimeBlocks,
    const std::vector<ManualCourseSelection>& manualSelections)
{
    const ScheduleConstraints scheduleConstraints =
        toScheduleConstraints(profileConstraints,
                              userAvoidTimeBlocks,
                              manualSelections);

    return Scheduler::makeSchedule(repository, scheduleConstraints);
}
