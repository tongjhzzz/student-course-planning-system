#include "scheduler.h"

#include <algorithm>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

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

// 为课程在某学期挑选第一个不冲突的教学班；全部冲突时返回 nullptr。
const CourseSection* pickSection(const CourseRepository& repository,
                                 const Course& course,
                                 const std::vector<PlannedCourse>& termCourses)
{
    const std::vector<CourseSection>* sections =
        repository.findSections(course.basicId);

    if (sections == nullptr) {
        return nullptr;
    }

    for (const CourseSection& section : *sections) {
        if (sectionFitsTerm(section, course, termCourses)) {
            return &section;
        }
    }

    return nullptr;
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

    // 尝试把课程排到不早于 earliestTerm 的某个学期。
    // 成功返回学期；失败返回 0，并把主要原因写入 failReason。
    int placeCourse(const Course& course, int earliestTerm, std::string& failReason)
    {
        bool triedAny = false;
        bool blockedByCredit = false;
        bool blockedByConflict = false;

        for (int term = std::max(1, earliestTerm); term <= 8; ++term) {
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
                pickSection(repository, course, result.coursesByTerm[term]);

            if (section == nullptr) {
                blockedByConflict = true;
                continue;
            }

            PlannedCourse planned{&course, section, term};
            result.coursesByTerm[term].push_back(planned);
            result.creditByTerm[term] += course.credit;
            placedTermById[course.basicId] = term;
            return term;
        }

        if (!triedAny) {
            failReason = "受先修顺序和开课季节限制，没有可用学期";
        } else if (blockedByCredit && !blockedByConflict) {
            failReason = "受每学期学分上限限制";
        } else if (blockedByConflict && !blockedByCredit) {
            failReason = "所有教学班均与同学期已选课程时间冲突";
        } else {
            failReason = "学分上限与时间冲突共同限制";
        }
        return 0;
    }
};

} // namespace

ScheduleResult Scheduler::makeSchedule(const CourseRepository& repository,
                                       const ScheduleConstraints& constraints)
{
    PlacementState state(repository, constraints);
    ScheduleResult& result = state.result;

    // ---------- 第一步：收集必修课，并补齐它们的全部先修课 ----------
    std::unordered_map<std::string, const Course*> requiredNodes;
    std::unordered_map<std::string, std::string> blockedReason; // 课程 ID -> 无法安排的原因
    std::queue<std::string> pending;
    int missingRequiredCount = 0;

    for (const std::string& id : constraints.requiredCourseIds) {
        pending.push(id);
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

    // ---------- 第二步：拓扑排序，按拓扑序贪心安排必修课 ----------
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
        // 后继课程必须排在前置课程之后（更晚的学期）。
        int earliestTerm = course->recommendedTerm;
        for (const std::string& prereq : course->prerequisiteIds) {
            auto termIt = state.placedTermById.find(prereq);
            if (termIt != state.placedTermById.end()) {
                earliestTerm = std::max(earliestTerm, termIt->second + 1);
            }
        }

        std::string failReason;
        if (state.placeCourse(*course, earliestTerm, failReason) == 0) {
            result.problems.push_back("课程 " + course->basicId + " " + course->name
                                      + " 无法安排：" + failReason + "。");
            ++unplacedRequiredCount;
        }
    }

    for (const auto& pair : blockedReason) {
        const Course* course = requiredNodes.at(pair.first);
        result.problems.push_back("课程 " + course->basicId + " " + course->name
                                  + " 无法安排：" + pair.second + "。");
        ++unplacedRequiredCount;
    }

    // ---------- 第三步：用选修候选池补足选修学分和总学分 ----------
    double placedTotal = 0.0;
    double placedElective = 0.0;
    for (int term = 1; term <= 8; ++term) {
        for (const PlannedCourse& planned : result.coursesByTerm[term]) {
            placedTotal += planned.course->credit;
            if (planned.course->category == "专业选修课") {
                placedElective += planned.course->credit;
            }
        }
    }

    double electiveNeed =
        std::max(0.0, constraints.electiveMinCredit - placedElective);
    double totalNeed = std::max(0.0, constraints.minTotalCredit - placedTotal);

    std::unordered_map<std::string, const Course*> electiveNodes;
    for (const std::string& id : constraints.electiveCandidateIds) {
        if (state.placedTermById.count(id) != 0) {
            continue; // 已作为必修课或先修课安排
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

    for (const Course* course : electiveOrder) {
        if (electiveNeed <= 0.0 && totalNeed <= 0.0) {
            break; // 学分已经补足
        }

        const bool isElective = course->category == "专业选修课";
        if (!isElective && totalNeed <= 0.0) {
            continue; // 非选修课只能补总学分，总学分已满则跳过
        }

        // 先修课必须已经全部排定，才能考虑这门选修课。
        int earliestTerm = course->recommendedTerm;
        bool prereqReady = true;
        for (const std::string& prereq : course->prerequisiteIds) {
            auto termIt = state.placedTermById.find(prereq);
            if (termIt == state.placedTermById.end()) {
                prereqReady = false;
                break;
            }
            earliestTerm = std::max(earliestTerm, termIt->second + 1);
        }
        if (!prereqReady) {
            continue;
        }

        std::string failReason;
        if (state.placeCourse(*course, earliestTerm, failReason) != 0) {
            totalNeed -= course->credit;
            if (isElective) {
                electiveNeed -= course->credit;
            }
        }
        // 某门选修课排不进去属于正常情况，直接从候选中跳过即可。
    }

    if (electiveNeed > 0.0) {
        result.problems.push_back("专业选修课学分不足：还差 "
                                  + formatCredit(electiveNeed) + " 学分。");
    }
    if (totalNeed > 0.0) {
        result.problems.push_back("总学分不足：还差 "
                                  + formatCredit(totalNeed) + " 学分。");
    }

    // ---------- 第四步：汇总统计并判定是否成功 ----------
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
                     && electiveNeed <= 0.0
                     && totalNeed <= 0.0;
    return result;
}
