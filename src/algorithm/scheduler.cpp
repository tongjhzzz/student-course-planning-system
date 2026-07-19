#include "scheduler.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

// 浮点学分比较时允许的误差。
constexpr double kEpsilon = 1e-6;

// 判断两门课程的上课周次是否有交集。
bool weeksOverlap(const Course& a, const Course& b)
{
    return a.beginWeek <= b.endWeek() && b.beginWeek <= a.endWeek();
}

// 判断一个教学班能否加入某学期：与已排课程在周次、星期、节次上都不冲突。
bool sectionFitsTerm(const CourseSection& section,
                     const Course& course,
                     const std::vector<PlannedCourse>& termCourses)
{
    for (const PlannedCourse& placed : termCourses) {
        if (!weeksOverlap(course, *placed.course)) {
            continue; // 周次不重叠的课程不算冲突
        }

        for (const TimeSlot& own : section.timeSlots) {
            for (const TimeSlot& other : placed.section->timeSlots) {
                if (own.overlaps(other)) {
                    return false;
                }
            }
        }
    }

    return true;
}

// 判断一个教学班是否与某个时间偏好块有节次重叠。
bool sectionOverlapsPreference(const CourseSection& section,
                               const TimePreferenceBlock& block)
{
    const TimeSlot blockSlot{block.day, block.beginPeriod, block.duration};
    for (const TimeSlot& slot : section.timeSlots) {
        if (slot.overlaps(blockSlot)) {
            return true;
        }
    }
    return false;
}

// 判断教学班是否触碰了硬性禁止时间（hard == true 的时间偏好块）。
bool violatesHardAvoidTime(
    const CourseSection& section,
    const std::vector<TimePreferenceBlock>& avoidBlocks)
{
    for (const TimePreferenceBlock& block : avoidBlocks) {
        if (block.hard && sectionOverlapsPreference(section, block)) {
            return true;
        }
    }
    return false;
}

// 计算一个教学班与“尽量避开时间”（hard == false）重叠的惩罚。
int calculateAvoidPenalty(
    const CourseSection& section,
    const std::vector<TimePreferenceBlock>& avoidBlocks)
{
    int penalty = 0;

    for (const TimePreferenceBlock& block : avoidBlocks) {
        if (block.hard) {
            continue;
        }

        if (sectionOverlapsPreference(section, block)) {
            ++penalty;
        }
    }

    return penalty;
}

// 为课程在某学期挑选最合适的教学班：
// 1. 排除与同学期课程冲突的教学班；
// 2. 排除违反 hard == true 时间偏好的教学班；
// 3. 在剩余教学班中选择软偏好惩罚最小的；
// 4. 惩罚相同时按教学班代码字典序选择，保证结果确定。
const CourseSection* pickBestSection(
    const CourseRepository& repository,
    const Course& course,
    const std::vector<PlannedCourse>& termCourses,
    const std::vector<TimePreferenceBlock>& avoidBlocks)
{
    const std::vector<CourseSection>* sections =
        repository.findSections(course.basicId);

    if (sections == nullptr) {
        return nullptr;
    }

    const CourseSection* bestSection = nullptr;
    int bestPenalty = std::numeric_limits<int>::max();

    for (const CourseSection& section : *sections) {
        if (!sectionFitsTerm(section, course, termCourses)) {
            continue;
        }

        if (violatesHardAvoidTime(section, avoidBlocks)) {
            continue;
        }

        const int penalty = calculateAvoidPenalty(section, avoidBlocks);

        if (bestSection == nullptr
            || penalty < bestPenalty
            || (penalty == bestPenalty
                && section.sectionId < bestSection->sectionId)) {
            bestSection = &section;
            bestPenalty = penalty;
        }
    }

    return bestSection;
}

// 对一组课程做拓扑排序（Kahn 算法）。
// ready 集合按（建议学期, 课程 ID）排序，保证同一数据下结果确定。
// 无法入排的剩余课程 ID 存入 cycleIds，即先修关系中的依赖环。
std::vector<const Course*> topoSort(
    const std::unordered_map<std::string, const Course*>& nodes,
    std::vector<std::string>& cycleIds)
{
    std::unordered_map<std::string, int> indegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;

    for (const auto& pair : nodes) {
        indegree[pair.first] = 0;
    }

    for (const auto& pair : nodes) {
        for (const std::string& prereq : pair.second->prerequisiteIds) {
            if (nodes.find(prereq) == nodes.end()) {
                continue; // 先修课不在本集合内，不构成集合内部的边
            }
            dependents[prereq].push_back(pair.first);
            ++indegree[pair.first];
        }
    }

    auto earlier = [&nodes](const std::string& a, const std::string& b) {
        const Course* courseA = nodes.at(a);
        const Course* courseB = nodes.at(b);
        if (courseA->recommendedTerm != courseB->recommendedTerm) {
            return courseA->recommendedTerm < courseB->recommendedTerm;
        }
        return a < b;
    };
    std::set<std::string, decltype(earlier)> ready(earlier);

    for (const auto& pair : indegree) {
        if (pair.second == 0) {
            ready.insert(pair.first);
        }
    }

    std::vector<const Course*> order;
    while (!ready.empty()) {
        const std::string id = *ready.begin();
        ready.erase(ready.begin());
        order.push_back(nodes.at(id));

        for (const std::string& next : dependents[id]) {
            if (--indegree[next] == 0) {
                ready.insert(next);
            }
        }
    }

    for (const auto& pair : indegree) {
        if (pair.second > 0) {
            cycleIds.push_back(pair.first);
        }
    }
    std::sort(cycleIds.begin(), cycleIds.end());

    return order;
}

// 把课程 ID 列表拼接成 "A、B、C" 形式，用于问题描述。
std::string joinIds(const std::vector<std::string>& ids)
{
    std::string text;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
            text += "、";
        }
        text += ids[i];
    }
    return text;
}

// 学分转字符串，去掉多余的尾零，例如 73 而不是 73.000000。
std::string formatCredit(double credit)
{
    std::ostringstream stream;
    stream << credit;
    return stream.str();
}

// 手动选课记录的文字描述，用于问题和提示信息。
std::string manualLabel(const ManualCourseSelection& selection)
{
    return selection.basicId + "（教学班 " + selection.sectionId + "）";
}

// 一次排课过程中共享的中间状态。
struct PlacementState
{
    PlacementState(const CourseRepository& repository,
                   const ScheduleConstraints& constraints)
        : repository(repository), constraints(constraints)
    {
    }

    const CourseRepository& repository;// 课程数据库
    const ScheduleConstraints& constraints;// 排课约束
    ScheduleResult result;// 正在填充的排课结果
    std::unordered_map<std::string, int> placedTermById; // 课程 ID -> 已排学期

    // 课程 ID -> 允许安排的最晚学期。由手动课程的先修关系反向推出：
    // 手动课程固定在第 t 学期时，它的先修课必须排在第 t-1 学期或更早。
    std::unordered_map<std::string, int> latestTermById;

    // 把课程放入结果并更新学分和已排记录。
    void commitPlacement(const Course& course,
                         const CourseSection& section,
                         int term)
    {
        PlannedCourse planned{&course, &section, term};
        result.coursesByTerm[term].push_back(planned);
        result.creditByTerm[term] += course.credit;
        placedTermById[course.basicId] = term;
    }

    // 选中的教学班仍与软时间偏好重叠时，写入一条提示（不影响成功判定）。
    void warnIfSoftAvoided(const Course& course, const CourseSection& section)
    {
        if (calculateAvoidPenalty(section, constraints.avoidTimeBlocks) > 0) {
            result.warnings.push_back(
                "提示：课程 " + course.basicId + " " + course.name
                + " 未能完全避开用户设置的时间偏好，已选择冲突最少的教学班。");
        }
    }

    // 尝试把课程排到 [earliestTerm, latestTerm] 范围内的某个学期。
    // 成功返回学期；失败返回 0，并把主要原因写入 failReason。
    int placeCourse(const Course& course,
                    int earliestTerm,
                    int latestTerm,
                    std::string& failReason)
    {
        bool triedAny = false;
        bool blockedByCredit = false;
        bool blockedBySection = false;

        const int firstTerm = std::max(1, earliestTerm);
        const int lastTerm = std::min(8, latestTerm);

        for (int term = firstTerm; term <= lastTerm; ++term) {
            if (!course.isAvailableInTerm(term)) {
                continue; // 开课季节不匹配，例如春季课不能排在奇数学期
            }
            triedAny = true;

            if (result.creditByTerm[term] + course.credit
                > constraints.maxCreditPerTerm[term]) {
                blockedByCredit = true;
                continue;
            }

            const CourseSection* section =
                pickBestSection(repository, course, result.coursesByTerm[term],
                                constraints.avoidTimeBlocks);

            if (section == nullptr) {
                blockedBySection = true;
                continue;
            }

            commitPlacement(course, *section, term);
            warnIfSoftAvoided(course, *section);
            return term;
        }

        if (!triedAny) {
            failReason = "受先修顺序和开课季节限制，没有可用学期";
        } else if (blockedByCredit && !blockedBySection) {
            failReason = "受每学期学分上限限制";
        } else if (blockedBySection && !blockedByCredit) {
            failReason = "所有教学班均不可用（与同学期课程冲突或触及严格禁止的时间）";
        } else {
            failReason = "学分上限与教学班可用性共同限制";
        }

        if (latestTerm < 8) {
            failReason += "（该课是手动课程的先修课，必须安排在第 "
                          + std::to_string(latestTerm) + " 学期或更早）";
        }
        return 0;
    }

    // 把指定课程的指定教学班精确放入指定学期（手动选课专用）。
    // 成功返回 true；失败返回 false 并把原因写入 failReason。
    bool placeCourseInTerm(const Course& course,
                           const CourseSection& section,
                           int term,
                           std::string& failReason)
    {
        if (term < 1 || term > 8) {
            failReason = "目标学期无效";
            return false;
        }
        if (placedTermById.count(course.basicId) != 0) {
            failReason = "该课程已经安排过，不能重复安排";
            return false;
        }
        if (!course.isAvailableInTerm(term)) {
            failReason = "该课程不能在第 " + std::to_string(term)
                         + " 学期开设，无法按用户指定学期安排";
            return false;
        }
        if (result.creditByTerm[term] + course.credit
            > constraints.maxCreditPerTerm[term]) {
            failReason = "加入后第 " + std::to_string(term)
                         + " 学期学分将超过上限 "
                         + formatCredit(constraints.maxCreditPerTerm[term])
                         + " 学分";
            return false;
        }
        if (violatesHardAvoidTime(section, constraints.avoidTimeBlocks)) {
            failReason = "教学班与严格禁止安排的时间重叠";
            return false;
        }

        for (const PlannedCourse& placed : result.coursesByTerm[term]) {
            if (!weeksOverlap(course, *placed.course)) {
                continue;
            }
            for (const TimeSlot& own : section.timeSlots) {
                for (const TimeSlot& other : placed.section->timeSlots) {
                    if (own.overlaps(other)) {
                        failReason = "与第 " + std::to_string(term)
                                     + " 学期已安排的课程 "
                                     + placed.course->basicId + "（教学班 "
                                     + placed.section->sectionId + "）时间冲突";
                        return false;
                    }
                }
            }
        }

        commitPlacement(course, section, term);
        return true;
    }

    // 尝试把课程排到指定的某一学期，教学班自动挑选（用于补足该学期最低学分）。
    // 成功返回 true；课程该学期不开课、超学分上限或教学班不可用时返回 false。
    bool placeCourseInTerm(const Course& course, int term)
    {
        if (!course.isAvailableInTerm(term)) {
            return false;
        }
        if (result.creditByTerm[term] + course.credit
            > constraints.maxCreditPerTerm[term]) {
            return false;
        }

        const CourseSection* section =
            pickBestSection(repository, course, result.coursesByTerm[term],
                            constraints.avoidTimeBlocks);
        if (section == nullptr) {
            return false;
        }

        commitPlacement(course, *section, term);
        warnIfSoftAvoided(course, *section);
        return true;
    }
};

} // namespace

ManualPlanCheckResult Scheduler::validateManualPlan(
    const CourseRepository& repository,
    const ScheduleConstraints& constraints)
{
    ManualPlanCheckResult check;

    std::set<std::string> seenCourseIds; // 已经出现过的基础课程 ID
    std::array<std::vector<PlannedCourse>, 9> placedByTerm; // 已通过校验的手动课程
    std::array<double, 9> creditByTerm{};

    for (const ManualCourseSelection& selection : constraints.manualSelections) {
        const std::string label = manualLabel(selection);

        // ① 学期是否合法
        if (selection.term < 1 || selection.term > 8) {
            check.problems.push_back("手动选课 " + label + " 的目标学期无效。");
            continue;
        }

        // ⑤ 同一门基础课程只能选择一个教学班和一个学期
        if (seenCourseIds.count(selection.basicId) != 0) {
            check.problems.push_back("手动选课中课程 " + selection.basicId
                                     + " 被重复选择，请只保留一个教学班和一个学期。");
            continue;
        }
        seenCourseIds.insert(selection.basicId);

        // ② 课程是否存在
        const Course* course = repository.findCourse(selection.basicId);
        if (course == nullptr) {
            check.problems.push_back("手动选课中的课程 " + selection.basicId
                                     + " 不存在。");
            continue;
        }

        // ③ 教学班是否存在
        const CourseSection* section =
            repository.findSection(selection.basicId, selection.sectionId);
        if (section == nullptr) {
            check.problems.push_back("课程 " + selection.basicId
                                     + " 不存在教学班 " + selection.sectionId + "。");
            continue;
        }

        // ④ 课程能否在该学期开设
        if (!course->isAvailableInTerm(selection.term)) {
            check.problems.push_back("课程 " + selection.basicId + " 不能在第 "
                                     + std::to_string(selection.term)
                                     + " 学期开设，无法按用户指定学期安排。");
            continue;
        }

        // ⑧ 必须遵守硬性时间禁用（软偏好不阻止手动选课）
        if (violatesHardAvoidTime(*section, constraints.avoidTimeBlocks)) {
            check.problems.push_back("手动选课 " + label
                                     + " 与严格禁止安排的时间重叠。");
            continue;
        }

        // ⑥ 手动课程之间不能时间冲突
        bool conflict = false;
        for (const PlannedCourse& placed : placedByTerm[selection.term]) {
            if (!weeksOverlap(*course, *placed.course)) {
                continue;
            }
            for (const TimeSlot& own : section->timeSlots) {
                for (const TimeSlot& other : placed.section->timeSlots) {
                    if (own.overlaps(other)) {
                        check.problems.push_back(
                            "第 " + std::to_string(selection.term) + " 学期手动课程 "
                            + selection.basicId + "（" + selection.sectionId + "）与 "
                            + placed.course->basicId + "（"
                            + placed.section->sectionId + "）上课时间冲突。");
                        conflict = true;
                        break;
                    }
                }
                if (conflict) {
                    break;
                }
            }
            if (conflict) {
                break;
            }
        }
        if (conflict) {
            continue;
        }

        // ⑦ 手动课程不能超过学期最高学分
        const double newCredit = creditByTerm[selection.term] + course->credit;
        if (newCredit > constraints.maxCreditPerTerm[selection.term]) {
            check.problems.push_back(
                "第 " + std::to_string(selection.term) + " 学期手动选择课程共 "
                + formatCredit(newCredit) + " 学分，超过上限 "
                + formatCredit(constraints.maxCreditPerTerm[selection.term])
                + " 学分。");
            continue;
        }

        PlannedCourse planned{course, section, selection.term};
        placedByTerm[selection.term].push_back(planned);
        creditByTerm[selection.term] += course->credit;
    }

    check.valid = check.problems.empty();
    return check;
}

ScheduleResult Scheduler::makeSchedule(const CourseRepository& repository,
                                       const ScheduleConstraints& constraints)
{
    PlacementState state(repository, constraints);
    ScheduleResult& result = state.result;

    // ---------- 第一步：校验并固定安排用户手动选择的课程 ----------
    // 手动选课是固定约束：必须按指定教学班、指定学期安排，
    // 不可行时明确报错，不换班也不换学期。
    bool manualPlanOk = true;

    for (const ManualCourseSelection& selection : constraints.manualSelections) {
        const std::string label = manualLabel(selection);

        if (selection.term < 1 || selection.term > 8) {
            result.problems.push_back("手动选课 " + label + " 的目标学期无效。");
            manualPlanOk = false;
            continue;
        }

        const Course* course = repository.findCourse(selection.basicId);
        if (course == nullptr) {
            result.problems.push_back("手动选课中的课程 " + selection.basicId
                                      + " 不存在。");
            manualPlanOk = false;
            continue;
        }

        const CourseSection* section =
            repository.findSection(selection.basicId, selection.sectionId);
        if (section == nullptr) {
            result.problems.push_back("课程 " + selection.basicId
                                      + " 不存在教学班 " + selection.sectionId
                                      + "。");
            manualPlanOk = false;
            continue;
        }

        if (state.placedTermById.count(selection.basicId) != 0) {
            result.problems.push_back("手动选课中课程 " + selection.basicId
                                      + " 被重复选择，请只保留一个教学班和一个学期。");
            manualPlanOk = false;
            continue;
        }

        std::string failReason;
        if (!state.placeCourseInTerm(*course, *section, selection.term,
                                     failReason)) {
            result.problems.push_back("手动选课 " + label
                                      + " 无法安排：" + failReason + "。");
            manualPlanOk = false;
            continue;
        }

        // 软偏好不阻止手动选课，但保留一条提示。
        if (calculateAvoidPenalty(*section, constraints.avoidTimeBlocks) > 0) {
            result.warnings.push_back("提示：手动课程 " + label
                                      + " 位于用户尽量避开的时间。");
        }
    }

    // 手动课程固定在第 t 学期时，它的先修课必须排在更早学期。
    // 沿先修边反向传播，为每门先修课算出允许的最晚学期。
    {
        std::queue<std::pair<std::string, int>> pendingBounds; // (课程 ID, 最晚学期)
        for (const ManualCourseSelection& selection : constraints.manualSelections) {
            if (state.placedTermById.count(selection.basicId) == 0) {
                continue; // 未能固定的手动课程不参与先修推导
            }
            const Course* course = repository.findCourse(selection.basicId);
            if (course == nullptr) {
                continue;
            }
            for (const std::string& prereq : course->prerequisiteIds) {
                pendingBounds.push({prereq, selection.term - 1});
            }
        }

        while (!pendingBounds.empty()) {
            const auto front = pendingBounds.front();
            pendingBounds.pop();

            auto it = state.latestTermById.find(front.first);
            if (it != state.latestTermById.end() && it->second <= front.second) {
                continue; // 已经有更严格的限制
            }
            state.latestTermById[front.first] = front.second;

            const Course* course = repository.findCourse(front.first);
            if (course == nullptr) {
                continue;
            }
            for (const std::string& prereq : course->prerequisiteIds) {
                pendingBounds.push({prereq, front.second - 1});
            }
        }
    }

    // ---------- 第二步：收集必修课（含手动课程），并补齐全部先修课 ----------
    std::unordered_map<std::string, const Course*> requiredNodes;
    std::unordered_map<std::string, std::string> blockedReason; // 课程 ID -> 无法安排的原因
    std::queue<std::string> pending;
    int missingRequiredCount = 0;

    for (const std::string& id : constraints.requiredCourseIds) {
        pending.push(id);
    }
    // 手动课程也加入课程图，它们的先修课才会被自动补齐；
    // 手动课程本身已在第一步排定，拓扑安排时会跳过。
    for (const ManualCourseSelection& selection : constraints.manualSelections) {
        if (state.placedTermById.count(selection.basicId) != 0) {
            pending.push(selection.basicId);
        }
    }

    while (!pending.empty()) {
        const std::string id = pending.front();
        pending.pop();

        if (requiredNodes.find(id) != requiredNodes.end()) {
            continue;
        }

        const Course* course = repository.findCourse(id);
        if (course == nullptr) {
            result.problems.push_back("课程 " + id + " 在课程数据中不存在。");
            ++missingRequiredCount;
            continue;
        }
        requiredNodes[id] = course;

        for (const std::string& prereq : course->prerequisiteIds) {
            if (repository.findCourse(prereq) == nullptr) {
                blockedReason[id] = "缺少前置课 " + prereq + "（课程数据中不存在）";
            } else {
                pending.push(prereq); // 先修课也必须排入课表
            }
        }
    }

    // 前置课无法安排时，后继课程同样无法安排，逐层向外传播。
    bool spread = true;
    while (spread) {
        spread = false;
        for (const auto& pair : requiredNodes) {
            if (blockedReason.count(pair.first) != 0) {
                continue;
            }
            for (const std::string& prereq : pair.second->prerequisiteIds) {
                if (blockedReason.count(prereq) != 0) {
                    blockedReason[pair.first] = "前置课 " + prereq + " 无法安排";
                    spread = true;
                    break;
                }
            }
        }
    }

    // ---------- 第三步：拓扑排序，按拓扑序贪心安排必修课 ----------
    std::unordered_map<std::string, const Course*> schedulable;
    for (const auto& pair : requiredNodes) {
        if (blockedReason.count(pair.first) == 0) {
            schedulable.insert(pair);
        }
    }

    std::vector<std::string> cycleIds;
    const std::vector<const Course*> order = topoSort(schedulable, cycleIds);

    if (!cycleIds.empty()) {
        result.problems.push_back("检测到先修关系依赖环，涉及课程："
                                  + joinIds(cycleIds) + "。");
    }

    int unplacedRequiredCount = static_cast<int>(cycleIds.size());

    for (const Course* course : order) {
        if (state.placedTermById.count(course->basicId) != 0) {
            continue; // 手动课程已在第一步固定安排，不能重复安排
        }

        // 后继课程必须排在前置课程之后（更晚的学期）。
        int earliestTerm = course->recommendedTerm;
        for (const std::string& prereq : course->prerequisiteIds) {
            auto termIt = state.placedTermById.find(prereq);
            if (termIt != state.placedTermById.end()) {
                earliestTerm = std::max(earliestTerm, termIt->second + 1);
            }
        }

        // 若该课是手动课程的先修课，则必须排在手动课程所在学期之前。
        int latestTerm = 8;
        auto latestIt = state.latestTermById.find(course->basicId);
        if (latestIt != state.latestTermById.end()) {
            latestTerm = std::min(latestTerm, latestIt->second);
        }

        std::string failReason;
        if (state.placeCourse(*course, earliestTerm, latestTerm, failReason)
            == 0) {
            result.problems.push_back("课程 " + course->basicId + " "
                                      + course->name + " 无法安排：" + failReason
                                      + "。");
            ++unplacedRequiredCount;
        }
    }

    for (const auto& pair : blockedReason) {
        if (state.placedTermById.count(pair.first) != 0) {
            continue; // 手动课程的先修问题由最后的先修顺序检查统一报告
        }
        const Course* course = requiredNodes.at(pair.first);
        result.problems.push_back("课程 " + course->basicId + " " + course->name
                                  + " 无法安排：" + pair.second + "。");
        ++unplacedRequiredCount;
    }

    // ---------- 汇总当前已排学分，准备选修候选池 ----------
    auto placedCredits = [&result](double& total, double& elective) {
        total = 0.0;
        elective = 0.0;
        for (int term = 1; term <= 8; ++term) {
            for (const PlannedCourse& planned : result.coursesByTerm[term]) {
                total += planned.course->credit;
                if (planned.course->category == "专业选修课") {
                    elective += planned.course->credit;
                }
            }
        }
    };

    std::unordered_map<std::string, const Course*> electiveNodes;
    for (const std::string& id : constraints.electiveCandidateIds) {
        if (state.placedTermById.count(id) != 0) {
            continue; // 已作为必修课、先修课或手动课程安排
        }
        const Course* course = repository.findCourse(id);
        if (course != nullptr) {
            electiveNodes[id] = course;
        }
    }

    std::vector<std::string> electiveCycleIds;
    const std::vector<const Course*> electiveOrder =
        topoSort(electiveNodes, electiveCycleIds);

    if (!electiveCycleIds.empty()) {
        result.problems.push_back("选修候选池中存在先修依赖环，已跳过："
                                  + joinIds(electiveCycleIds) + "。");
    }

    // ---------- 第四步：逐学期补足最低学分 ----------
    // 从未安排的选修候选课中挑选补入本学期的课程：
    // 本学期可开、先修课已排在更早学期、不超学分上限、时间不与已排课程冲突、
    // 尽量满足时间偏好。
    bool minTermCreditMet = true;
    for (int term = 1; term <= 8; ++term) {
        const double minCredit = constraints.minCreditPerTerm[term];
        if (result.creditByTerm[term] + kEpsilon >= minCredit) {
            continue;
        }

        for (const Course* course : electiveOrder) {
            if (result.creditByTerm[term] + kEpsilon >= minCredit) {
                break; // 本学期学分已经补足
            }
            if (state.placedTermById.count(course->basicId) != 0) {
                continue; // 已安排
            }

            bool prereqReady = true;
            for (const std::string& prereq : course->prerequisiteIds) {
                auto termIt = state.placedTermById.find(prereq);
                if (termIt == state.placedTermById.end()
                    || termIt->second >= term) {
                    prereqReady = false;
                    break;
                }
            }
            if (!prereqReady) {
                continue;
            }

            state.placeCourseInTerm(*course, term);
            // 补不进去属于正常情况，继续尝试下一门候选课。
        }

        if (result.creditByTerm[term] + kEpsilon < minCredit) {
            minTermCreditMet = false;
            result.problems.push_back(
                "第 " + std::to_string(term) + " 学期最低学分不足：当前 "
                + formatCredit(result.creditByTerm[term]) + " 学分，要求至少 "
                + formatCredit(minCredit) + " 学分，还差 "
                + formatCredit(minCredit - result.creditByTerm[term]) + " 学分。");
        }
    }

    // ---------- 第五步：用选修候选池补足专业选修学分和总学分 ----------
    double placedTotal = 0.0;
    double placedElective = 0.0;
    placedCredits(placedTotal, placedElective);

    double electiveNeed =
        std::max(0.0, constraints.electiveMinCredit - placedElective);
    double totalNeed = std::max(0.0, constraints.minTotalCredit - placedTotal);

    // 尝试把一门选修候选课排入课表；成功返回 true。
    auto tryPlaceFromPool = [&state](const Course* course) {
        if (state.placedTermById.count(course->basicId) != 0) {
            return false; // 已安排
        }

        // 先修课必须已经全部排定，才能考虑这门选修课。
        int earliestTerm = course->recommendedTerm;
        for (const std::string& prereq : course->prerequisiteIds) {
            auto termIt = state.placedTermById.find(prereq);
            if (termIt == state.placedTermById.end()) {
                return false;
            }
            earliestTerm = std::max(earliestTerm, termIt->second + 1);
        }

        std::string failReason;
        return state.placeCourse(*course, earliestTerm, 8, failReason) != 0;
    };

    // 先补足专业选修课学分（专业选修课同时也计入总学分）。
    for (const Course* course : electiveOrder) {
        if (electiveNeed <= kEpsilon) {
            break;
        }
        if (course->category != "专业选修课") {
            continue;
        }
        if (tryPlaceFromPool(course)) {
            electiveNeed -= course->credit;
            totalNeed -= course->credit;
        }
        // 某门选修课排不进去属于正常情况，直接从候选中跳过即可。
    }

    // 再补足总学分（候选池中非专业选修课只能补总学分）。
    for (const Course* course : electiveOrder) {
        if (totalNeed <= kEpsilon) {
            break;
        }
        if (tryPlaceFromPool(course)) {
            totalNeed -= course->credit;
            if (course->category == "专业选修课") {
                electiveNeed -= course->credit;
            }
        }
    }

    if (electiveNeed > kEpsilon) {
        result.problems.push_back("专业选修课学分不足：还差 "
                                  + formatCredit(electiveNeed) + " 学分。");
    }
    if (totalNeed > kEpsilon) {
        result.problems.push_back("总学分不足：还差 "
                                  + formatCredit(totalNeed) + " 学分。");
    }

    // ---------- 第六步：检查所有已排课程的先修顺序 ----------
    bool prereqOrderOk = true;
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            for (const std::string& prereq : planned.course->prerequisiteIds) {
                auto termIt = state.placedTermById.find(prereq);
                if (termIt == state.placedTermById.end()
                    || termIt->second >= term) {
                    prereqOrderOk = false;
                    result.problems.push_back(
                        "课程 " + planned.course->basicId + " "
                        + planned.course->name + " 的先修课 " + prereq
                        + " 未能安排在更早学期。");
                }
            }
        }
    }

    // ---------- 第七步：汇总统计并判定是否成功 ----------
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            result.totalCredit += planned.course->credit;
            if (planned.course->category == "专业选修课") {
                result.electiveCredit += planned.course->credit;
            } else {
                result.requiredBasicCredit += planned.course->credit;
            }
        }
    }

    result.success = missingRequiredCount == 0
                     && unplacedRequiredCount == 0
                     && electiveNeed <= kEpsilon
                     && totalNeed <= kEpsilon
                     && minTermCreditMet
                     && manualPlanOk
                     && prereqOrderOk;
    return result;
}
