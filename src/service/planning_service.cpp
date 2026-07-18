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
    ScheduleConstraints scheduleConstraints;
    scheduleConstraints.requiredCourseIds = constraints.requiredCourseIds;
    scheduleConstraints.electiveCandidateIds =
        constraints.electiveCandidateCourseIds;
    scheduleConstraints.minTotalCredit = constraints.minTotalCredit;
    scheduleConstraints.electiveMinCredit = constraints.electiveMinCredit;

    // PlanningConstraints 的下标 0~7 对应第 1~8 学期；
    // ScheduleConstraints 的下标 1~8 对应第 1~8 学期。
    for (int term = 1; term <= 8; ++term) {
        scheduleConstraints.maxCreditPerTerm[term] =
            constraints.maxCreditPerSemester[term - 1];
    }

    return scheduleConstraints;
}

ScheduleResult PlanningService::createSchedule(
    const CourseRepository& repository,
    const PlanningConstraints& constraints)
{
    const ScheduleConstraints scheduleConstraints =
        toScheduleConstraints(constraints);

    return Scheduler::makeSchedule(repository, scheduleConstraints);
}
