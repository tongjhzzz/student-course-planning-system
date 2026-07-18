#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "../data/course_repository.h"
#include "../model/planning_constraints.h"

#include <QMainWindow>

#include <string>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QTabWidget;
class ScheduleResult;

// 智能课程规划系统的主窗口。
class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void createInterface();
    void createTimetableTable(QTableWidget* table);
    void loadProfileFiles();
    void generateSchedule();
    void showScheduleResult(const ScheduleResult& result);
    void clearScheduleTables();

    std::string selectedProfilePath() const;
    static QString formatCourseText(const Course& course,
                                    const CourseSection& section,
                                    const TimeSlot& slot);
    static QString courseColor(const std::string& courseId);

    QComboBox* profileComboBox = nullptr;
    QPushButton* generateButton = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* dataSummaryLabel = nullptr;
    QLabel* resultSummaryLabel = nullptr;
    QTabWidget* termTabWidget = nullptr;
    QTableWidget* tablesByTerm[8]{};
    QTextEdit* problemsTextEdit = nullptr;

    CourseRepository repository;
    PlanningConstraints constraints;
};

#endif // MAIN_WINDOW_H
