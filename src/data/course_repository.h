#ifndef COURSE_REPOSITORY_H
#define COURSE_REPOSITORY_H

#include "../model/course.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// 统一保存从 CSV 文件读取到的全部课程和教学班数据。
class CourseRepository// 课程仓库
{
public:
    // 添加一门基础课程。课程 ID 为空或已经存在时返回 false。
    bool addCourse(const Course& course);

    // 添加一个教学班。对应的基础课程不存在或教学班重复时返回 false。
    bool addSection(const CourseSection& section);

    // 为指定教学班添加一段上课时间。教学班不存在时返回 false。
    bool addTimeSlot(const std::string& basicId,
                     const std::string& sectionId,
                     const TimeSlot& timeSlot);

    // 按基础课程 ID 查找课程；找不到时返回 nullptr。
    const Course* findCourse(const std::string& basicId) const;

    // 按“基础课程 ID + 教学班 ID”查找教学班；找不到时返回 nullptr。
    const CourseSection* findSection(const std::string& basicId,
                                     const std::string& sectionId) const;

    // 查找一门课程的所有教学班；找不到时返回 nullptr。
    const std::vector<CourseSection>* findSections(
        const std::string& basicId) const;

    // 返回全部基础课程，供后续课程查询界面使用。
    std::vector<const Course*> allCourses() const;

    std::size_t courseCount() const;
    std::size_t sectionCount() const;

    // 清空当前保存的所有数据。
    void clear();

private:
    // key: course_basic_ID
    std::unordered_map<std::string, Course> coursesById;// 通过课程id快速查看课程

    // key: course_basic_ID
    // value: 该基础课程的全部教学班
    std::unordered_map<std::string, std::vector<CourseSection>> sectionsByCourseId;// 通过课程id查找这门课的全部可选教学班

    std::size_t totalSectionCount = 0;

    // 查找可修改的教学班，仅供本类内部添加上课时间时使用。
    CourseSection* findSectionForUpdate(const std::string& basicId,
                                        const std::string& sectionId);
};

#endif // COURSE_REPOSITORY_H
