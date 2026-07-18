#include "main_window.h"

#include "../algorithm/scheduler.h"
#include "../service/planning_service.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
constexpr int kPeriodCount = 13;
constexpr int kColumnCount = 8;
constexpr int kTimeColumn = 0;
constexpr int kWeekdayStartColumn = 1;

const char* const kPeriodTimes[kPeriodCount] = {
    "8:00 ~ 8:45", "8:50 ~ 9:35", "9:50 ~ 10:35",
    "10:40 ~ 11:25", "11:30 ~ 12:15", "13:00 ~ 13:45",
    "13:50 ~ 14:35", "14:50 ~ 15:35", "15:40 ~ 16:25",
    "16:30 ~ 17:15", "18:00 ~ 18:45", "18:50 ~ 19:35",
    "19:40 ~ 20:25"
};

std::string projectDirectory()
{
    return PROJECT_SOURCE_DIRECTORY;
}

QColor backgroundColorForPeriod(int period)
{
    if (period <= 5) {
        return QColor("#E8F5E5"); // 上午：浅绿色
    }
    if (period <= 10) {
        return QColor("#FFF5CF"); // 下午：浅黄色
    }
    return QColor("#FCE5E4");     // 晚上：浅红色
}

QString timetableCellStyle(const QString& backgroundColor)
{
    return "QLabel {"
           "background-color: " + backgroundColor + ";"
           "border: 1px solid #8B98A1;"
           "border-radius: 4px;"
           "padding: 4px;"
           "color: #1F2933;"
           "}";
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    createInterface();
    loadProfileFiles();

    setWindowTitle("智能课程规划系统");
    resize(1320, 930);
}

void MainWindow::createInterface()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(8);

    QHBoxLayout* controlLayout = new QHBoxLayout;
    QLabel* profileLabel = new QLabel("选择专业培养方案：", this);
    profileComboBox = new QComboBox(this);
    generateButton = new QPushButton("生成八学期规划", this);

    controlLayout->addWidget(profileLabel);
    controlLayout->addWidget(profileComboBox, 1);
    controlLayout->addWidget(generateButton);
    mainLayout->addLayout(controlLayout);

    statusLabel = new QLabel("请选择培养方案，然后点击“生成八学期规划”。", this);
    dataSummaryLabel = new QLabel("课程数据尚未加载。", this);
    resultSummaryLabel = new QLabel("尚未生成规划结果。", this);

    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(dataSummaryLabel);
    mainLayout->addWidget(resultSummaryLabel);

    termTabWidget = new QTabWidget(this);

    for (int term = 1; term <= 8; ++term) {
        QTableWidget* table = new QTableWidget(this);
        createTimetableTable(table);

        tablesByTerm[term - 1] = table;
        termTabWidget->addTab(table, "第 " + QString::number(term) + " 学期");
    }

    mainLayout->addWidget(termTabWidget, 1);

    QLabel* problemsLabel = new QLabel("规划问题或提示：", this);
    problemsTextEdit = new QTextEdit(this);
    problemsTextEdit->setReadOnly(true);
    problemsTextEdit->setPlaceholderText("生成规划后，如果存在无法满足的约束，会在这里显示原因。");
    problemsTextEdit->setMaximumHeight(105);

    mainLayout->addWidget(problemsLabel);
    mainLayout->addWidget(problemsTextEdit);

    connect(generateButton, &QPushButton::clicked,
            this, [this]() { generateSchedule(); });
}

void MainWindow::createTimetableTable(QTableWidget* table)
{
    const QStringList headers = {
        "时间", "周一", "周二", "周三", "周四", "周五", "周六", "周日"
    };

    table->clearSpans();
    table->clearContents();
    table->setRowCount(kPeriodCount);
    table->setColumnCount(kColumnCount);
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(true);
    table->setWordWrap(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->horizontalHeader()->setSectionResizeMode(kTimeColumn, QHeaderView::Fixed);
    table->setColumnWidth(kTimeColumn, 125);

    for (int column = kWeekdayStartColumn; column < kColumnCount; ++column) {
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
    }

    for (int row = 0; row < kPeriodCount; ++row) {
        const int period = row + 1;
        const QColor background = backgroundColorForPeriod(period);
        table->setRowHeight(row, 70);

        for (int column = 0; column < kColumnCount; ++column) {
            QTableWidgetItem* item = new QTableWidgetItem;
            item->setBackground(background);
            item->setTextAlignment(Qt::AlignCenter);

            if (column == kTimeColumn) {
                item->setText("第 " + QString::number(period) + " 节\n"
                              + kPeriodTimes[row]);
            }

            table->setItem(row, column, item);
        }
    }

    table->setStyleSheet(
        "QTableWidget {"
        "gridline-color: #9AA7B0;"
        "border: 1px solid #9AA7B0;"
        "font-size: 12px;"
        "}"
        "QHeaderView::section {"
        "background-color: #E5EDF2;"
        "border: 1px solid #9AA7B0;"
        "padding: 6px;"
        "font-weight: bold;"
        "}");
}

void MainWindow::loadProfileFiles()
{
    const QString directoryPath = QString::fromStdString(projectDirectory())
        + "/data/major_profiles";
    const QDir profileDirectory(directoryPath);
    const QStringList fileNames = profileDirectory.entryList(
        {"*.json"}, QDir::Files, QDir::Name);

    for (const QString& fileName : fileNames) {
        const QString profilePath = profileDirectory.filePath(fileName);
        const QString displayName = QFileInfo(fileName).completeBaseName();
        profileComboBox->addItem(displayName, profilePath);
    }

    if (profileComboBox->count() == 0) {
        statusLabel->setText("未找到 data/major_profiles 中的培养方案 JSON 文件。");
        generateButton->setEnabled(false);
    }
}

std::string MainWindow::selectedProfilePath() const
{
    return profileComboBox->currentData().toString().toStdString();
}

void MainWindow::generateSchedule()
{
    const std::string courseInfoPath = projectDirectory() + "/data/course_info.csv";
    const std::string courseTimePath = projectDirectory() + "/data/course_time.csv";
    const std::string profilePath = selectedProfilePath();
    std::string errorMessage;

    generateButton->setEnabled(false);
    statusLabel->setText("正在读取数据并生成课程规划，请稍候……");

    if (!PlanningService::loadPlanningData(courseInfoPath,
                                           courseTimePath,
                                           profilePath,
                                           repository,
                                           constraints,
                                           errorMessage)) {
        generateButton->setEnabled(true);
        statusLabel->setText("加载数据失败。请查看提示信息。");
        QMessageBox::critical(this, "加载失败",
                              "无法加载课程数据或培养方案：\n"
                              + QString::fromStdString(errorMessage));
        return;
    }

    const ScheduleResult result =
        PlanningService::createSchedule(repository, constraints);
    showScheduleResult(result);

    generateButton->setEnabled(true);
}

void MainWindow::showScheduleResult(const ScheduleResult& result)
{
    clearScheduleTables();

    dataSummaryLabel->setText(
        "培养方案：" + QString::fromStdString(constraints.profileName)
        + "　|　基础课程：" + QString::number(repository.courseCount())
        + "　|　教学班：" + QString::number(repository.sectionCount()));

    resultSummaryLabel->setText(
        "生成状态：" + QString(result.success ? "成功" : "未完全满足约束")
        + "　|　总学分：" + QString::number(result.totalCredit)
        + "　|　专业选修课学分：" + QString::number(result.electiveCredit));

    statusLabel->setText(result.success
                              ? "课程规划已生成。可在第 1～8 学期标签页查看周课表。"
                              : "课程规划已生成，但存在未满足的约束。请查看下方问题说明。");

    for (int term = 1; term <= 8; ++term) {
        QTableWidget* table = tablesByTerm[term - 1];
        const std::vector<PlannedCourse>& plannedCourses =
            result.coursesByTerm[term];

        for (const PlannedCourse& planned : plannedCourses) {
            const Course& course = *planned.course;
            const CourseSection& section = *planned.section;

            // 一个教学班可能有多个上课时间段，因此每个时间段都显示为一个课程块。
            for (const TimeSlot& slot : section.timeSlots) {
                if (slot.day < 0 || slot.day > 6 || slot.beginPeriod < 1
                    || slot.beginPeriod > kPeriodCount || slot.duration <= 0) {
                    continue;
                }

                const int row = slot.beginPeriod - 1;
                const int column = kWeekdayStartColumn + slot.day;
                const int spanRows = std::min(slot.duration, kPeriodCount - row);

                table->setSpan(row, column, spanRows, 1);

                QLabel* courseLabel = new QLabel(formatCourseText(course, section, slot), table);
                courseLabel->setAlignment(Qt::AlignCenter);
                courseLabel->setWordWrap(true);
                courseLabel->setStyleSheet(timetableCellStyle(courseColor(course.basicId)));
                courseLabel->setToolTip(courseLabel->text());

                table->setCellWidget(row, column, courseLabel);
            }
        }

        termTabWidget->setTabText(
            term - 1,
            "第 " + QString::number(term) + " 学期（"
            + QString::number(result.creditByTerm[term]) + " 学分）");
    }

    QStringList problemLines;
    for (const std::string& problem : result.problems) {
        problemLines.append("- " + QString::fromStdString(problem));
    }

    if (problemLines.isEmpty()) {
        problemsTextEdit->setText("没有发现未满足的规划约束。");
    } else {
        problemsTextEdit->setText(problemLines.join('\n'));
    }
}

void MainWindow::clearScheduleTables()
{
    for (QTableWidget* table : tablesByTerm) {
        createTimetableTable(table);
    }

    problemsTextEdit->clear();
}

QString MainWindow::formatCourseText(const Course& course,
                                     const CourseSection& section,
                                     const TimeSlot& slot)
{
    return QString::fromStdString(course.name)
        + "\n" + QString::fromStdString(section.sectionId)
        + "\n第 " + QString::number(course.beginWeek)
        + "-" + QString::number(course.endWeek()) + " 周"
        + " · 第 " + QString::number(slot.beginPeriod)
        + "-" + QString::number(slot.endPeriod()) + " 节"
        + "\n" + QString::fromStdString(section.classroom)
        + "\n" + QString::fromStdString(section.teacher);
}

QString MainWindow::courseColor(const std::string& courseId)
{
    // 同一门课的所有时间段使用相同颜色；不同课程循环使用一组柔和颜色。
    static const QString colors[] = {
        "#F4E7C6", // 浅米色
        "#E5D7F2", // 淡紫色
        "#F7D5C7", // 浅橘粉色
        "#CDEEE2", // 淡青绿色
        "#CCE7F5", // 淡天蓝色
        "#DCEFC7", // 浅草绿
        "#DAE4F1", // 浅灰蓝色
        "#F6DFC1"  // 浅杏色
    };

    unsigned int value = 0;
    for (unsigned char character : courseId) {
        value = value * 31U + character;
    }

    const unsigned int colorCount = sizeof(colors) / sizeof(colors[0]);
    return colors[value % colorCount];
}
