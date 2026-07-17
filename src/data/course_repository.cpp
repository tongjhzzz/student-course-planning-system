#include "course_repository.h"

bool CourseRepository::addCourse(const Course& course)// 添加课程
{
    if (course.basicId.empty()) {
        return false;
    }

    if (coursesById.find(course.basicId) != coursesById.end()) {// 课程已经存在
        return false;
    }

    coursesById[course.basicId] = course;
    return true;
}

bool CourseRepository::addSection(const CourseSection& section)// 添加教学班
{
    if (section.basicId.empty() || section.sectionId.empty()) {
        return false;
    }

    if (findCourse(section.basicId) == nullptr) {// 先检查这个教学班所属的基础课程是否已经存在；如果不存在，就不能添加该教学班
        return false;
    }

    if (findSection(section.basicId, section.sectionId) != nullptr) {// 检查相同的“课程 ID + 教学班 ID”是否已经存在；若已经存在，说明重复添加
        return false;
    }

    sectionsByCourseId[section.basicId].push_back(section);
    ++totalSectionCount;

    return true;
}

bool CourseRepository::addTimeSlot(const std::string& basicId,
                                   const std::string& sectionId,
                                   const TimeSlot& timeSlot)
{// 为指定教学班添加一段上课时间。教学班不存在时返回 false
    if (timeSlot.day < 0 || timeSlot.day > 6
        || timeSlot.beginPeriod <= 0 || timeSlot.duration <= 0) {
        return false;
    }

    CourseSection* section = findSectionForUpdate(basicId, sectionId);
    if (section == nullptr) {
        return false;
    }

    section->timeSlots.push_back(timeSlot);
    return true;
}

const Course* CourseRepository::findCourse(const std::string& basicId) const
{
    auto it = coursesById.find(basicId);

    if (it == coursesById.end()) {
        return nullptr;
    }

    return &it->second;// unordered_map中first对应的是key，second对应的是value
}

const CourseSection* CourseRepository::findSection(
    const std::string& basicId,
    const std::string& sectionId) const
{
    auto it = sectionsByCourseId.find(basicId);

    if (it == sectionsByCourseId.end()) {
        return nullptr;
    }

    for (const CourseSection& section : it->second) {
        if (section.sectionId == sectionId) {
            return &section;
        }
    }

    return nullptr;
}

const std::vector<CourseSection>* CourseRepository::findSections(
    const std::string& basicId) const
{
    auto it = sectionsByCourseId.find(basicId);

    if (it == sectionsByCourseId.end()) {
        return nullptr;
    }

    return &it->second;
}

std::vector<const Course*> CourseRepository::allCourses() const
{
    std::vector<const Course*> result;
    result.reserve(coursesById.size());

    for (const auto& pair : coursesById) {
        result.push_back(&pair.second);
    }

    return result;
}

std::size_t CourseRepository::courseCount() const
{
    return coursesById.size();
}

std::size_t CourseRepository::sectionCount() const
{
    return totalSectionCount;
}

void CourseRepository::clear()
{
    coursesById.clear();
    sectionsByCourseId.clear();
    totalSectionCount = 0;
}

CourseSection* CourseRepository::findSectionForUpdate(
    const std::string& basicId,
    const std::string& sectionId)
{// 在仓库中找到一个具体教学班，并返回“可修改”的教学班地址,用于内部更新
    auto it = sectionsByCourseId.find(basicId);

    if (it == sectionsByCourseId.end()) {
        return nullptr;
    }

    for (CourseSection& section : it->second) {
        if (section.sectionId == sectionId) {
            return &section;
        }
    }

    return nullptr;
}
