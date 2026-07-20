#include "course.h"

#include <algorithm>

// 用开始节次+持续节次来计算结束节次
int TimeSlot::endPeriod() const
{
    return beginPeriod + duration - 1;// 注意有一个 - 1
}

// 判断当前时间段和other时间段有没有时间上的重合冲突
bool TimeSlot::overlaps(const TimeSlot& other) const
{
    if (day != other.day) {
        return false;
    }

    return std::max(beginPeriod, other.beginPeriod)
           <= std::min(endPeriod(), other.endPeriod());// 如果两个课程之间存在交集那么开始节次较晚的那个不晚于结束节次更早的那个，这样就会有重叠
}

// 用开始周+持续周来计算在哪周结束
int Course::endWeek() const
{
    return beginWeek + durationWeeks - 1;
}

// 通过学期编号的奇偶来判断是在秋季学期还是春季学期
bool Course::isAvailableInTerm(int term) const
{
    if (term < 1 || term > 8 || term < recommendedTerm) {
        return false;
    }

    bool isAutumnTerm = term % 2 == 1;// 奇数表示的是秋季学期，偶数表示的是春季学期

    if (semester == "秋季学期") {
        return isAutumnTerm;
    }

    if (semester == "春季学期") {
        return !isAutumnTerm;
    }

    return false;
}

// 用课程ID+教学班ID返回每个教学班特定的ID
std::string CourseSection::uniqueKey() const
{
    return basicId + "#" + sectionId;
}
