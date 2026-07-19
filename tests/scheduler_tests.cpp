// 排课算法（Scheduler）与规划服务（PlanningService）的单元测试。
// 测试场景对应《接口需求说明》第 11 节的测试用例表。
//
// 这是一个纯控制台程序：以非 0 退出码表示存在失败用例。

#include "../src/algorithm/scheduler.h"
#include "../src/service/planning_service.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool condition, const std::string& name)
{
    if (condition) {
        ++g_passed;
        std::cout << "[通过] " << name << '\n';
    } else {
        ++g_failed;
        std::cout << "[失败] " << name << '\n';
    }
}

// ---------- 测试数据构造辅助 ----------

Course makeCourse(const std::string& id,
                  double credit,
                  const std::string& category,
                  const std::string& semester,
                  int recommendedTerm,
                  std::vector<std::string> prereqs = {})
{
    Course course;
    course.basicId = id;
    course.name = "课程" + id;
    course.department = "计算机学院";
    course.semester = semester;
    course.recommendedTerm = recommendedTerm;
    course.category = category;
    course.credit = credit;
    course.beginWeek = 1;
    course.durationWeeks = 16;
    course.prerequisiteIds = std::move(prereqs);
    return course;
}

TimeSlot slot(int day, int beginPeriod, int duration)
{
    return TimeSlot{day, beginPeriod, duration};
}

void addSection(CourseRepository& repository,
                const std::string& basicId,
                const std::string& sectionId,
                std::vector<TimeSlot> slots)
{
    CourseSection section;
    section.basicId = basicId;
    section.sectionId = sectionId;
    section.teacher = "教师";
    section.classroom = "教室101";
    section.capacity = 100;
    section.timeSlots = std::move(slots);
    repository.addSection(section);
}

TimePreferenceBlock avoidBlock(int day,
                               int beginPeriod,
                               int duration,
                               bool hard)
{
    TimePreferenceBlock block;
    block.day = day;
    block.beginPeriod = beginPeriod;
    block.duration = duration;
    block.hard = hard;
    block.reason = "测试时间偏好";
    return block;
}

ScheduleConstraints baseConstraints()
{
    ScheduleConstraints constraints;
    for (int term = 1; term <= 8; ++term) {
        constraints.maxCreditPerTerm[term] = 40.0;
    }
    return constraints;
}

// ---------- 结果检查辅助 ----------

int termOf(const ScheduleResult& result, const std::string& basicId)
{
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            if (planned.course->basicId == basicId) {
                return term;
            }
        }
    }
    return -1;
}

std::string sectionOf(const ScheduleResult& result, const std::string& basicId)
{
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            if (planned.course->basicId == basicId) {
                return planned.section->sectionId;
            }
        }
    }
    return "";
}

bool containsMessage(const std::vector<std::string>& messages,
                     const std::string& needle)
{
    for (const std::string& message : messages) {
        if (message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// ---------- 用例 1：普通培养方案自动排课 ----------

void testBasicAutoSchedule()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("MA101", 4, "学科基础课", "秋季学期", 1));
    addSection(repository, "MA101", "01", {slot(0, 1, 2)});
    repository.addCourse(
        makeCourse("CS201", 4, "专业必修课", "秋季学期", 3, {"MA101"}));
    addSection(repository, "CS201", "01", {slot(1, 1, 2)});
    repository.addCourse(makeCourse("CS202", 4, "专业必修课", "春季学期", 2));
    addSection(repository, "CS202", "01", {slot(2, 1, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.requiredCourseIds = {"MA101", "CS201", "CS202"};
    constraints.minTotalCredit = 12.0;

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);

    check(result.success, "1.1 普通自动排课成功");
    check(result.problems.empty(), "1.2 普通自动排课无问题信息");
    check(termOf(result, "MA101") == 1, "1.3 MA101 安排在第 1 学期");
    check(termOf(result, "CS201") == 3, "1.4 CS201 安排在第 3 学期");
    check(termOf(result, "CS202") == 2, "1.5 CS202 安排在第 2 学期");
    check(result.totalCredit == 12.0, "1.6 总学分统计正确");
    check(termOf(result, "CS201") > termOf(result, "MA101"),
          "1.7 先修关系顺序正确");
}

// ---------- 用例 2：某学期最低学分无法满足 ----------

void testMinTermCreditShortage()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("MA101", 4, "学科基础课", "秋季学期", 1));
    addSection(repository, "MA101", "01", {slot(0, 1, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.requiredCourseIds = {"MA101"};
    constraints.minCreditPerTerm[1] = 16.0; // 第 1 学期至少 16 学分
    constraints.minTotalCredit = 4.0;

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);

    check(!result.success, "2.1 最低学分不足时规划失败");
    check(containsMessage(result.problems, "第 1 学期最低学分不足"),
          "2.2 指出是哪个学期学分不足");
    check(containsMessage(result.problems, "还差 12"),
          "2.3 指出还差多少学分");
}

// ---------- 用例 3：多个教学班可选时优先避开软偏好 ----------

void testSoftPreferenceSectionChoice()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("CS301", 4, "专业必修课", "秋季学期", 1));
    addSection(repository, "CS301", "01", {slot(0, 1, 2)}); // 周一 1-2 节
    addSection(repository, "CS301", "02", {slot(1, 1, 2)}); // 周二 1-2 节

    ScheduleConstraints constraints = baseConstraints();
    constraints.requiredCourseIds = {"CS301"};
    constraints.minTotalCredit = 4.0;
    constraints.avoidTimeBlocks = {avoidBlock(0, 1, 2, false)}; // 软偏好：避开周一

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);

    check(result.success, "3.1 有软偏好时排课仍然成功");
    check(sectionOf(result, "CS301") == "02",
          "3.2 优先选择避开软偏好的教学班");

    ScheduleConstraints noPreference = baseConstraints();
    noPreference.requiredCourseIds = {"CS301"};
    noPreference.minTotalCredit = 4.0;
    const ScheduleResult plainResult =
        Scheduler::makeSchedule(repository, noPreference);
    check(sectionOf(plainResult, "CS301") == "01",
          "3.3 无偏好时按字典序选择教学班");
}

// ---------- 用例 4：教学班撞上 hard=true 时间块 ----------

void testHardPreferenceExclusion()
{
    // 4a：存在可替代教学班时，硬偏好只排除撞上的教学班。
    {
        CourseRepository repository;
        repository.addCourse(
            makeCourse("CS301", 4, "专业必修课", "秋季学期", 1));
        addSection(repository, "CS301", "01", {slot(0, 1, 2)});
        addSection(repository, "CS301", "02", {slot(1, 1, 2)});

        ScheduleConstraints constraints = baseConstraints();
        constraints.requiredCourseIds = {"CS301"};
        constraints.minTotalCredit = 4.0;
        constraints.avoidTimeBlocks = {avoidBlock(0, 1, 2, true)};

        const ScheduleResult result =
            Scheduler::makeSchedule(repository, constraints);

        check(result.success, "4.1 硬偏好下存在替代班时排课成功");
        check(sectionOf(result, "CS301") == "02",
              "4.2 撞上 hard=true 时间块的教学班不被选择");
    }

    // 4b：唯一教学班被硬偏好排除时，课程无法安排。
    {
        CourseRepository repository;
        repository.addCourse(
            makeCourse("CS302", 4, "专业必修课", "秋季学期", 1));
        addSection(repository, "CS302", "01", {slot(0, 1, 2)});

        ScheduleConstraints constraints = baseConstraints();
        constraints.requiredCourseIds = {"CS302"};
        constraints.minTotalCredit = 4.0;
        constraints.avoidTimeBlocks = {avoidBlock(0, 1, 2, true)};

        const ScheduleResult result =
            Scheduler::makeSchedule(repository, constraints);

        check(!result.success, "4.3 唯一教学班被硬偏好排除时规划失败");
        check(termOf(result, "CS302") == -1,
              "4.4 被硬偏好排除的教学班不会排入课表");
    }
}

// ---------- 用例 5：一条合法手动选课被固定安排 ----------

void testManualSelectionFixed()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("MA101", 4, "学科基础课", "秋季学期", 1));
    addSection(repository, "MA101", "01", {slot(0, 1, 2)});
    repository.addCourse(makeCourse("CS401", 4, "专业选修课", "春季学期", 2));
    addSection(repository, "CS401", "01", {slot(0, 1, 2)});
    addSection(repository, "CS401", "02", {slot(1, 1, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.requiredCourseIds = {"MA101"};
    constraints.electiveCandidateIds = {"CS401"};
    constraints.minTotalCredit = 8.0;
    constraints.electiveMinCredit = 4.0;
    constraints.manualSelections = {{"CS401", "02", 2}};

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);

    check(result.success, "5.1 合法手动选课时排课成功");
    check(termOf(result, "CS401") == 2, "5.2 手动课程固定在指定学期");
    check(sectionOf(result, "CS401") == "02",
          "5.3 手动课程固定在指定教学班");
    check(result.electiveCredit == 4.0, "5.4 手动选修课计入专业选修学分");
    check(result.totalCredit == 8.0, "5.5 手动课程计入总学分");
}

// ---------- 用例 6：手动课程之间时间冲突 ----------

CourseRepository buildConflictRepository()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("CS501", 4, "专业必修课", "秋季学期", 1));
    addSection(repository, "CS501", "02", {slot(0, 1, 2)}); // 周一 1-2 节
    repository.addCourse(makeCourse("CS502", 4, "专业必修课", "秋季学期", 1));
    addSection(repository, "CS502", "03", {slot(0, 2, 2)}); // 周一 2-3 节，与上一门重叠
    return repository;
}

void testManualConflict()
{
    CourseRepository repository = buildConflictRepository();

    ScheduleConstraints constraints = baseConstraints();
    constraints.manualSelections = {{"CS501", "02", 1}, {"CS502", "03", 1}};

    const ManualPlanCheckResult checkResult =
        Scheduler::validateManualPlan(repository, constraints);
    check(!checkResult.valid, "6.1 手动课程冲突时校验不通过");
    check(containsMessage(checkResult.problems, "CS501")
              && containsMessage(checkResult.problems, "CS502")
              && containsMessage(checkResult.problems, "冲突"),
          "6.2 校验结果明确列出冲突课程");

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);
    check(!result.success, "6.3 手动课程冲突时规划失败");
    check(containsMessage(result.problems, "CS501")
              && containsMessage(result.problems, "CS502"),
          "6.4 规划结果明确列出冲突课程");
}

// ---------- 用例 7：手动课程的学期与开课季节不符 ----------

void testManualWrongTerm()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("CS601", 4, "专业必修课", "春季学期", 2));
    addSection(repository, "CS601", "01", {slot(0, 1, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.manualSelections = {{"CS601", "01", 1}}; // 春季课排到第 1 学期

    const ManualPlanCheckResult checkResult =
        Scheduler::validateManualPlan(repository, constraints);
    check(!checkResult.valid, "7.1 学期不符时校验不通过");
    check(containsMessage(checkResult.problems, "不能在第 1 学期开设"),
          "7.2 校验结果说明课程不能在该学期开设");

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);
    check(!result.success, "7.3 学期不符时规划失败");
    check(containsMessage(result.problems, "不能在第 1 学期开设"),
          "7.4 规划结果明确说明原因");
}

// ---------- 用例 8：同一基础课程被手动选择两次 ----------

void testManualDuplicateCourse()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("MA101", 4, "学科基础课", "秋季学期", 1));
    addSection(repository, "MA101", "01", {slot(0, 1, 2)});
    addSection(repository, "MA101", "02", {slot(1, 1, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.manualSelections = {{"MA101", "01", 1}, {"MA101", "02", 3}};

    const ManualPlanCheckResult checkResult =
        Scheduler::validateManualPlan(repository, constraints);
    check(!checkResult.valid, "8.1 重复选择时校验不通过");
    check(containsMessage(checkResult.problems, "被重复选择"),
          "8.2 校验结果明确说明重复课程");

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);
    check(!result.success, "8.3 重复选择时规划失败");
    check(containsMessage(result.problems, "被重复选择"),
          "8.4 规划结果明确说明重复课程");
}

// ---------- 用例 9：手动课程依赖先修课 ----------

CourseRepository buildPrereqRepository()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("CS701", 4, "学科基础课", "秋季学期", 1));
    addSection(repository, "CS701", "01", {slot(0, 1, 2)});
    repository.addCourse(
        makeCourse("CS702", 4, "专业必修课", "秋季学期", 3, {"CS701"}));
    addSection(repository, "CS702", "01", {slot(1, 1, 2)});
    repository.addCourse(
        makeCourse("CS703", 4, "专业必修课", "秋季学期", 5, {"CS702"}));
    addSection(repository, "CS703", "01", {slot(2, 1, 2)});
    return repository;
}

void testManualPrereq()
{
    // 9a：手动课程固定在第 5 学期，先修课应自动安排在更早学期。
    {
        CourseRepository repository = buildPrereqRepository();

        ScheduleConstraints constraints = baseConstraints();
        constraints.minTotalCredit = 12.0;
        constraints.manualSelections = {{"CS703", "01", 5}};

        const ScheduleResult result =
            Scheduler::makeSchedule(repository, constraints);

        check(result.success, "9.1 先修课可满足时规划成功");
        check(termOf(result, "CS703") == 5, "9.2 手动课程固定在第 5 学期");
        check(termOf(result, "CS701") == 1 && termOf(result, "CS702") == 3,
              "9.3 先修课被自动安排在更早学期");
    }

    // 9b：先修课最早只能与手动课同学期开设，先修顺序无法满足。
    {
        CourseRepository repository;
        repository.addCourse(
            makeCourse("CS702", 4, "专业必修课", "秋季学期", 5));
        addSection(repository, "CS702", "01", {slot(1, 1, 2)});
        repository.addCourse(
            makeCourse("CS703", 4, "专业必修课", "秋季学期", 5, {"CS702"}));
        addSection(repository, "CS703", "01", {slot(2, 1, 2)});

        ScheduleConstraints constraints = baseConstraints();
        constraints.minTotalCredit = 8.0;
        constraints.manualSelections = {{"CS703", "01", 5}};

        const ScheduleResult result =
            Scheduler::makeSchedule(repository, constraints);

        check(!result.success, "9.4 先修链无法满足时规划失败");
        check(containsMessage(result.problems, "先修课"),
              "9.5 规划结果说明先修课无法满足");
    }
}

// ---------- 用例 10：手动课程让某学期学分超过上限 ----------

void testManualExceedsMaxCredit()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("CS801", 20, "专业必修课", "秋季学期", 1));
    addSection(repository, "CS801", "01", {slot(0, 1, 2)});
    repository.addCourse(makeCourse("CS802", 25, "专业必修课", "秋季学期", 1));
    addSection(repository, "CS802", "01", {slot(1, 1, 2)});

    ScheduleConstraints constraints = baseConstraints(); // 上限 40 学分
    constraints.manualSelections = {{"CS801", "01", 1}, {"CS802", "01", 1}};

    const ManualPlanCheckResult checkResult =
        Scheduler::validateManualPlan(repository, constraints);
    check(!checkResult.valid, "10.1 手动课程超上限时校验不通过");
    check(containsMessage(checkResult.problems, "第 1 学期手动选择课程共 45 学分")
              && containsMessage(checkResult.problems, "超过上限 40"),
          "10.2 校验结果说明第几学期超出多少");

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);
    check(!result.success, "10.3 手动课程超上限时规划失败");
    check(containsMessage(result.problems, "超过上限")
              || containsMessage(result.problems, "学分将超过上限"),
          "10.4 规划结果说明超出上限");
}

// ---------- 用例 11：手动选择软偏好时间中的课程 ----------

void testManualSoftPreferenceWarning()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("CS901", 4, "专业必修课", "秋季学期", 1));
    addSection(repository, "CS901", "01", {slot(0, 1, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.minTotalCredit = 4.0;
    constraints.avoidTimeBlocks = {avoidBlock(0, 1, 2, false)}; // 软偏好
    constraints.manualSelections = {{"CS901", "01", 1}};

    const ScheduleResult result =
        Scheduler::makeSchedule(repository, constraints);

    check(result.success, "11.1 手动课程撞上软偏好仍允许排入");
    check(containsMessage(result.warnings, "手动课程 CS901"),
          "11.2 结果中加入提示信息");
    check(result.problems.empty(), "11.3 软偏好不产生失败原因");
}

// ---------- 用例 12：结果确定性 ----------

void testDeterminism()
{
    CourseRepository repository;
    repository.addCourse(makeCourse("MA101", 4, "学科基础课", "秋季学期", 1));
    addSection(repository, "MA101", "01", {slot(0, 1, 2)});
    repository.addCourse(
        makeCourse("CS201", 4, "专业必修课", "秋季学期", 3, {"MA101"}));
    addSection(repository, "CS201", "01", {slot(1, 1, 2)});
    addSection(repository, "CS201", "02", {slot(2, 1, 2)});
    repository.addCourse(makeCourse("CS202", 4, "专业必修课", "春季学期", 2));
    addSection(repository, "CS202", "01", {slot(0, 3, 2)});
    repository.addCourse(makeCourse("EL301", 4, "专业选修课", "秋季学期", 1));
    addSection(repository, "EL301", "01", {slot(3, 1, 2)});
    addSection(repository, "EL301", "02", {slot(4, 1, 2)});
    repository.addCourse(makeCourse("EL302", 4, "专业选修课", "春季学期", 2));
    addSection(repository, "EL302", "01", {slot(3, 3, 2)});

    ScheduleConstraints constraints = baseConstraints();
    constraints.requiredCourseIds = {"MA101", "CS201", "CS202"};
    constraints.electiveCandidateIds = {"EL301", "EL302"};
    constraints.minTotalCredit = 16.0;
    constraints.minCreditPerTerm[1] = 8.0;
    constraints.minCreditPerTerm[2] = 8.0;
    constraints.avoidTimeBlocks = {avoidBlock(1, 1, 2, false)};

    const ScheduleResult first =
        Scheduler::makeSchedule(repository, constraints);
    const ScheduleResult second =
        Scheduler::makeSchedule(repository, constraints);

    auto serialize = [](const ScheduleResult& result) {
        std::string text;
        for (int term = 1; term <= 8; ++term) {
            for (const PlannedCourse& planned : result.coursesByTerm[term]) {
                text += std::to_string(term) + ":"
                        + planned.course->basicId + "#"
                        + planned.section->sectionId + ";";
            }
        }
        return text;
    };

    check(first.success && second.success, "12.1 确定性测试排课成功");
    check(serialize(first) == serialize(second),
          "12.2 同一输入多次运行结果一致");
    check(first.problems == second.problems
              && first.warnings == second.warnings,
          "12.3 同一输入多次运行问题与提示一致");
}

// ---------- PlanningService 约束转换 ----------

void testPlanningServiceConversion()
{
    PlanningConstraints profile;
    profile.requiredCourseIds = {"MA101"};
    profile.electiveCandidateCourseIds = {"EL301"};
    for (int index = 0; index < 8; ++index) {
        profile.minCreditPerSemester[index] = 16.0;
        profile.maxCreditPerSemester[index] = 40.0;
    }
    profile.minCreditPerSemester[6] = 12.0;
    profile.minCreditPerSemester[7] = 8.0;
    profile.minTotalCredit = 120.0;
    profile.electiveMinCredit = 20.0;
    profile.avoidTimeBlocks = {avoidBlock(2, 5, 2, false)};

    const std::vector<TimePreferenceBlock> userBlocks = {
        avoidBlock(0, 1, 1, false), avoidBlock(4, 9, 2, true)};
    const std::vector<ManualCourseSelection> manualSelections = {
        {"EL301", "01", 3}};

    const ScheduleConstraints converted = PlanningService::toScheduleConstraints(
        profile, userBlocks, manualSelections);

    check(converted.minCreditPerTerm[1] == 16.0
              && converted.minCreditPerTerm[7] == 12.0
              && converted.minCreditPerTerm[8] == 8.0,
          "13.1 每学期最低学分下标转换正确");
    check(converted.maxCreditPerTerm[3] == 40.0,
          "13.2 每学期最高学分下标转换正确");
    check(converted.avoidTimeBlocks.size() == 3
              && converted.avoidTimeBlocks[0].day == 2
              && converted.avoidTimeBlocks[1].day == 0
              && converted.avoidTimeBlocks[2].hard,
          "13.3 培养方案与用户时间偏好按顺序合并");
    check(converted.manualSelections.size() == 1
              && converted.manualSelections[0].basicId == "EL301"
              && converted.manualSelections[0].sectionId == "01"
              && converted.manualSelections[0].term == 3,
          "13.4 手动选课原样传入算法约束");

    // 旧接口保持可用：不带用户偏好和手动选课时等价于空列表。
    const ScheduleConstraints legacy =
        PlanningService::toScheduleConstraints(profile);
    check(legacy.avoidTimeBlocks.size() == 1
              && legacy.manualSelections.empty(),
          "13.5 旧接口等价于空偏好与空手动的调用");
}

} // namespace

int main()
{
    testBasicAutoSchedule();
    testMinTermCreditShortage();
    testSoftPreferenceSectionChoice();
    testHardPreferenceExclusion();
    testManualSelectionFixed();
    testManualConflict();
    testManualWrongTerm();
    testManualDuplicateCourse();
    testManualPrereq();
    testManualExceedsMaxCredit();
    testManualSoftPreferenceWarning();
    testDeterminism();
    testPlanningServiceConversion();

    std::cout << "\n共 " << (g_passed + g_failed) << " 项检查：通过 "
              << g_passed << " 项，失败 " << g_failed << " 项。\n";

    return g_failed == 0 ? 0 : 1;
}
