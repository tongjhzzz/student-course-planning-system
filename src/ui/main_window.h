#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "../algorithm/scheduler.h"
#include "../data/course_repository.h"
#include "../model/planning_constraints.h"
#include "../model/manual_course_selection.h"

#include <QMainWindow>

#include <array>
#include <string>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QTabWidget;
class QWidget;

// 智能课程规划系统的主窗口。
class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void createInterface();
    void createTimetableTable(QTableWidget* table);
    void createCourseQueryPage();
    void createTimePreferencePage();
    void createManualCoursePlanPage();
    void loadCourseData();
    void refreshCourseQueryOptions();
    void queryCourses();
    void resetCourseQuery();
    void addSelectedCourseToManualPlan();
    void refreshManualCoursePlanTable();
    void cancelSelectedManualCourse();
    void clearManualCoursePlan();
    void loadManualCoursePlan();
    void saveManualCoursePlan();
    void createTimePreferenceTable();
    void toggleAvoidTimeSlot(int row, int column);
    void updateTimePreferenceCell(int day, int period);
    void updateTimePreferenceSummary();
    void loadTimePreferences();
    void saveTimePreferences();
    void clearTimePreferences();
    void loadProfileFiles();
    void generateSchedule();
    void exportSchedule();
    void showScheduleResult(const ScheduleResult& result);
    void clearScheduleTables();

    std::string selectedProfilePath() const;
    static QString formatCourseText(const Course& course,
                                    const CourseSection& section,
                                    const TimeSlot& slot);
    static QString formatSectionTimeSlots(const CourseSection& section);
    static QString courseColor(const std::string& courseId);

    QComboBox* profileComboBox = nullptr;
    QPushButton* generateButton = nullptr;
    QPushButton* exportButton = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* dataSummaryLabel = nullptr;
    QLabel* resultSummaryLabel = nullptr;
    QTabWidget* termTabWidget = nullptr;
    QTableWidget* tablesByTerm[8]{};
    QTextEdit* problemsTextEdit = nullptr;

    QWidget* courseQueryPage = nullptr;

    QWidget* timePreferencePage = nullptr;
    QTableWidget* timePreferenceTable = nullptr;
    QPushButton* saveTimePreferenceButton = nullptr;
    QPushButton* clearTimePreferenceButton = nullptr;
    QLabel* timePreferenceSummaryLabel = nullptr;
    QLineEdit* courseKeywordEdit = nullptr;
    QComboBox* categoryComboBox = nullptr;
    QComboBox* departmentComboBox = nullptr;
    QComboBox* semesterComboBox = nullptr;
    QDoubleSpinBox* minCreditSpinBox = nullptr;
    QDoubleSpinBox* maxCreditSpinBox = nullptr;
    QPushButton* queryButton = nullptr;
    QPushButton* resetQueryButton = nullptr;
    QLabel* queryCountLabel = nullptr;
    QTableWidget* courseQueryTable = nullptr;
    QComboBox* manualPlanTermComboBox = nullptr;
    QPushButton* addToManualPlanButton = nullptr;

    QWidget* manualCoursePlanPage = nullptr;
    QTableWidget* manualCoursePlanTable = nullptr;
    QLabel* manualPlanSummaryLabel = nullptr;
    QPushButton* cancelManualCourseButton = nullptr;
    QPushButton* clearManualPlanButton = nullptr;
    QPushButton* saveManualPlanButton = nullptr;

    CourseRepository repository;
    PlanningConstraints constraints;
    ScheduleResult currentScheduleResult;
    bool hasScheduleResult = false;

    // 下标 [星期][节次 - 1]；true 表示用户希望尽量避开该时间。
    std::array<std::array<bool, 13>, 7> avoidTimeSlots{};

    std::vector<ManualCourseSelection> manualCourseSelections;
};

#endif // MAIN_WINDOW_H
