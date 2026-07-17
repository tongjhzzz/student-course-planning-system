#include "course.h"

#include <algorithm>

int TimeSlot::endPeriod() const
{
    return beginPeriod + duration - 1;
}

bool TimeSlot::overlaps(const TimeSlot& other) const
{
    if (day != other.day) {
        return false;
    }

    return std::max(beginPeriod, other.beginPeriod)
           <= std::min(endPeriod(), other.endPeriod());// 如果两个课程之间存在交集那么开始节次较晚的那个不晚于结束节次更早的那个，这样就会有重叠
}

int Course::endWeek() const
{
    return beginWeek + durationWeeks - 1;
}

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

std::string CourseSection::uniqueKey() const
{
    return basicId + "#" + sectionId;
}
