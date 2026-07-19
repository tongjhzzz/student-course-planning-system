#include "main_window.h"

#include "../algorithm/scheduler.h"
#include "../data/csv_reader.h"
#include "../data/time_preference_storage.h"
#include "../service/planning_service.h"
#include "../output/schedule_exporter.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <set>
#include <vector>

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
    loadCourseData();
    loadProfileFiles();
    loadTimePreferences();

    setWindowTitle("智能课程规划系统");
    resize(1320, 930);
}

void MainWindow::loadCourseData()
{
    const std::string dataDirectory = projectDirectory() + "/data";
    const std::string courseInfoPath = dataDirectory + "/course_info.csv";
    const std::string courseTimePath = dataDirectory + "/course_time.csv";

    std::string errorMessage;

    repository.clear();

    if (!CsvReader::loadCourseInfo(courseInfoPath, repository, errorMessage)
        || !CsvReader::loadCourseTime(courseTimePath, repository, errorMessage)) {
        statusLabel->setText(
            "课程数据加载失败：" + QString::fromStdString(errorMessage));
        dataSummaryLabel->setText("课程数据尚未成功加载。");
        return;
    }

    dataSummaryLabel->setText(
        "已加载课程数据：基础课程 "
        + QString::number(repository.courseCount())
        + " 门，教学班 "
        + QString::number(repository.sectionCount())
        + " 个。");

    refreshCourseQueryOptions();
    queryCourses();

    statusLabel->setText(
        "课程数据已加载。请选择培养方案生成规划，或前往课程查询页查询课程。");
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
    exportButton = new QPushButton("导出当前规划 CSV", this);
    exportButton->setEnabled(false);

    controlLayout->addWidget(profileLabel);
    controlLayout->addWidget(profileComboBox, 1);
    controlLayout->addWidget(generateButton);
    controlLayout->addWidget(exportButton);
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

    createCourseQueryPage();
    termTabWidget->addTab(courseQueryPage, "课程查询");

    createTimePreferencePage();
    termTabWidget->addTab(timePreferencePage, "时间偏好");

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
    connect(exportButton, &QPushButton::clicked,
            this, [this]() { exportSchedule(); });
}

void MainWindow::createCourseQueryPage()
{
    courseQueryPage = new QWidget(this);
    QVBoxLayout* pageLayout = new QVBoxLayout(courseQueryPage);
    pageLayout->setContentsMargins(10, 10, 10, 10);
    pageLayout->setSpacing(8);

    QGridLayout* filterLayout = new QGridLayout;
    filterLayout->setHorizontalSpacing(8);
    filterLayout->setVerticalSpacing(6);

    courseKeywordEdit = new QLineEdit(courseQueryPage);
    courseKeywordEdit->setPlaceholderText("输入课程名称或课程编号的一部分");

    categoryComboBox = new QComboBox(courseQueryPage);
    departmentComboBox = new QComboBox(courseQueryPage);
    semesterComboBox = new QComboBox(courseQueryPage);

    minCreditSpinBox = new QDoubleSpinBox(courseQueryPage);
    minCreditSpinBox->setRange(0.0, 30.0);
    minCreditSpinBox->setDecimals(1);
    minCreditSpinBox->setSingleStep(0.5);
    minCreditSpinBox->setPrefix("最低 ");
    minCreditSpinBox->setSuffix(" 学分");

    maxCreditSpinBox = new QDoubleSpinBox(courseQueryPage);
    maxCreditSpinBox->setRange(0.0, 30.0);
    maxCreditSpinBox->setDecimals(1);
    maxCreditSpinBox->setSingleStep(0.5);
    maxCreditSpinBox->setPrefix("最高 ");
    maxCreditSpinBox->setSuffix(" 学分");
    maxCreditSpinBox->setValue(30.0);

    queryButton = new QPushButton("查询", courseQueryPage);
    resetQueryButton = new QPushButton("重置", courseQueryPage);

    filterLayout->addWidget(new QLabel("课程名称/编号：", courseQueryPage), 0, 0);
    filterLayout->addWidget(courseKeywordEdit, 0, 1, 1, 3);
    filterLayout->addWidget(new QLabel("课程类别：", courseQueryPage), 0, 4);
    filterLayout->addWidget(categoryComboBox, 0, 5);
    filterLayout->addWidget(new QLabel("开课院系：", courseQueryPage), 1, 0);
    filterLayout->addWidget(departmentComboBox, 1, 1);
    filterLayout->addWidget(new QLabel("开课季节：", courseQueryPage), 1, 2);
    filterLayout->addWidget(semesterComboBox, 1, 3);
    filterLayout->addWidget(minCreditSpinBox, 1, 4);
    filterLayout->addWidget(maxCreditSpinBox, 1, 5);
    filterLayout->addWidget(queryButton, 0, 6);
    filterLayout->addWidget(resetQueryButton, 1, 6);

    pageLayout->addLayout(filterLayout);

    queryCountLabel = new QLabel("课程数据加载后可查询全部教学班。", courseQueryPage);
    pageLayout->addWidget(queryCountLabel);

    courseQueryTable = new QTableWidget(courseQueryPage);
    const QStringList headers = {
        "课程编号", "课程名称", "教学班", "类别", "学分", "开课院系",
        "开课季节", "教师", "教室", "上课周次", "上课时间"
    };
    courseQueryTable->setColumnCount(headers.size());
    courseQueryTable->setHorizontalHeaderLabels(headers);
    courseQueryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    courseQueryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    courseQueryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    courseQueryTable->setAlternatingRowColors(true);
    courseQueryTable->setWordWrap(false);
    courseQueryTable->verticalHeader()->setVisible(false);
    courseQueryTable->horizontalHeader()->setStretchLastSection(true);
    courseQueryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    courseQueryTable->setStyleSheet(
        "QTableWidget { gridline-color: #AAB5BE; font-size: 12px; }"
        "QHeaderView::section { background-color: #E5EDF2; padding: 6px; "
        "border: 1px solid #9AA7B0; font-weight: bold; }");

    pageLayout->addWidget(courseQueryTable, 1);

    connect(queryButton, &QPushButton::clicked,
            this, [this]() { queryCourses(); });
    connect(resetQueryButton, &QPushButton::clicked,
            this, [this]() { resetCourseQuery(); });
    connect(courseKeywordEdit, &QLineEdit::returnPressed,
            this, [this]() { queryCourses(); });
}

void MainWindow::refreshCourseQueryOptions()
{
    if (categoryComboBox == nullptr || departmentComboBox == nullptr
        || semesterComboBox == nullptr) {
        return;
    }

    std::set<std::string> categories;
    std::set<std::string> departments;
    std::set<std::string> semesters;

    for (const Course* course : repository.allCourses()) {
        categories.insert(course->category);
        departments.insert(course->department);
        semesters.insert(course->semester);
    }

    categoryComboBox->clear();
    departmentComboBox->clear();
    semesterComboBox->clear();

    categoryComboBox->addItem("全部", "");
    departmentComboBox->addItem("全部", "");
    semesterComboBox->addItem("全部", "");

    for (const std::string& category : categories) {
        categoryComboBox->addItem(QString::fromStdString(category),
                                  QString::fromStdString(category));
    }
    for (const std::string& department : departments) {
        departmentComboBox->addItem(QString::fromStdString(department),
                                    QString::fromStdString(department));
    }
    for (const std::string& semester : semesters) {
        semesterComboBox->addItem(QString::fromStdString(semester),
                                  QString::fromStdString(semester));
    }
}

void MainWindow::queryCourses()
{
    if (courseQueryTable == nullptr) {
        return;
    }

    const QString keyword = courseKeywordEdit->text().trimmed();
    const QString category = categoryComboBox->currentData().toString();
    const QString department = departmentComboBox->currentData().toString();
    const QString semester = semesterComboBox->currentData().toString();
    const double minCredit = minCreditSpinBox->value();
    const double maxCredit = maxCreditSpinBox->value();

    courseQueryTable->setRowCount(0);

    if (minCredit > maxCredit) {
        queryCountLabel->setText("最低学分不能大于最高学分。请调整后重新查询。");
        return;
    }

    std::vector<const Course*> courses = repository.allCourses();
    std::sort(courses.begin(), courses.end(), [](const Course* left,
                                                 const Course* right) {
        return left->basicId < right->basicId;
    });

    int resultCount = 0;
    for (const Course* course : courses) {
        const QString courseId = QString::fromStdString(course->basicId);
        const QString courseName = QString::fromStdString(course->name);
        const QString searchableText = courseId + " " + courseName;

        if (!keyword.isEmpty()
            && !searchableText.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        if (!category.isEmpty()
            && category != QString::fromStdString(course->category)) {
            continue;
        }
        if (!department.isEmpty()
            && department != QString::fromStdString(course->department)) {
            continue;
        }
        if (!semester.isEmpty()
            && semester != QString::fromStdString(course->semester)) {
            continue;
        }
        if (course->credit < minCredit || course->credit > maxCredit) {
            continue;
        }

        const std::vector<CourseSection>* sections =
            repository.findSections(course->basicId);
        if (sections == nullptr || sections->empty()) {
            continue;
        }

        for (const CourseSection& section : *sections) {
            const int row = courseQueryTable->rowCount();
            courseQueryTable->insertRow(row);

            const QString weekText = "第 " + QString::number(course->beginWeek)
                + "-" + QString::number(course->endWeek()) + " 周";
            const QString values[] = {
                courseId,
                courseName,
                QString::fromStdString(section.sectionId),
                QString::fromStdString(course->category),
                QString::number(course->credit, 'f', 1),
                QString::fromStdString(course->department),
                QString::fromStdString(course->semester),
                QString::fromStdString(section.teacher),
                QString::fromStdString(section.classroom),
                weekText,
                formatSectionTimeSlots(section)
            };

            for (int column = 0; column < 11; ++column) {
                QTableWidgetItem* item = new QTableWidgetItem(values[column]);
                item->setTextAlignment(column == 4 ? Qt::AlignCenter
                                                   : Qt::AlignLeft | Qt::AlignVCenter);
                courseQueryTable->setItem(row, column, item);
            }

            ++resultCount;
        }
    }

    queryCountLabel->setText("共找到 " + QString::number(resultCount)
                              + " 个符合条件的教学班。");
}

void MainWindow::resetCourseQuery()
{
    courseKeywordEdit->clear();
    categoryComboBox->setCurrentIndex(0);
    departmentComboBox->setCurrentIndex(0);
    semesterComboBox->setCurrentIndex(0);
    minCreditSpinBox->setValue(0.0);
    maxCreditSpinBox->setValue(30.0);
    queryCourses();
}

void MainWindow::createTimePreferencePage()
{
    timePreferencePage = new QWidget(this);
    QVBoxLayout* pageLayout = new QVBoxLayout(timePreferencePage);
    pageLayout->setContentsMargins(10, 10, 10, 10);
    pageLayout->setSpacing(8);

    QLabel* descriptionLabel = new QLabel(
        "点击课程表中的时间格，标记你希望尽量避开上课的时间。"
        "红色格表示已选择。", timePreferencePage);
    descriptionLabel->setWordWrap(true);
    pageLayout->addWidget(descriptionLabel);

    timePreferenceTable = new QTableWidget(timePreferencePage);
    createTimePreferenceTable();
    pageLayout->addWidget(timePreferenceTable, 1);

    timePreferenceSummaryLabel = new QLabel(timePreferencePage);
    pageLayout->addWidget(timePreferenceSummaryLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    saveTimePreferenceButton = new QPushButton("保存时间偏好", timePreferencePage);
    clearTimePreferenceButton = new QPushButton("清空已选时间", timePreferencePage);

    buttonLayout->addWidget(saveTimePreferenceButton);
    buttonLayout->addWidget(clearTimePreferenceButton);
    buttonLayout->addStretch();
    pageLayout->addLayout(buttonLayout);

    connect(timePreferenceTable, &QTableWidget::cellClicked,
            this, [this](int row, int column) {
                toggleAvoidTimeSlot(row, column);
            });
    connect(saveTimePreferenceButton, &QPushButton::clicked,
            this, [this]() { saveTimePreferences(); });
    connect(clearTimePreferenceButton, &QPushButton::clicked,
            this, [this]() { clearTimePreferences(); });

    updateTimePreferenceSummary();
}

void MainWindow::createTimePreferenceTable()
{
    const QStringList headers = {
        "时间", "周一", "周二", "周三", "周四", "周五", "周六", "周日"
    };

    timePreferenceTable->clearContents();
    timePreferenceTable->setRowCount(kPeriodCount);
    timePreferenceTable->setColumnCount(kColumnCount);
    timePreferenceTable->setHorizontalHeaderLabels(headers);
    timePreferenceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    timePreferenceTable->setSelectionMode(QAbstractItemView::NoSelection);
    timePreferenceTable->setFocusPolicy(Qt::NoFocus);
    timePreferenceTable->setShowGrid(true);
    timePreferenceTable->verticalHeader()->setVisible(false);
    timePreferenceTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    timePreferenceTable->horizontalHeader()->setSectionResizeMode(
        kTimeColumn, QHeaderView::Fixed);
    timePreferenceTable->setColumnWidth(kTimeColumn, 125);

    for (int column = kWeekdayStartColumn; column < kColumnCount; ++column) {
        timePreferenceTable->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::Stretch);
    }

    for (int row = 0; row < kPeriodCount; ++row) {
        timePreferenceTable->setRowHeight(row, 48);

        QTableWidgetItem* timeItem = new QTableWidgetItem(
            "第 " + QString::number(row + 1) + " 节\n" + kPeriodTimes[row]);
        timeItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setBackground(QColor("#E5EDF2"));
        timePreferenceTable->setItem(row, kTimeColumn, timeItem);

        for (int day = 0; day < 7; ++day) {
            QTableWidgetItem* item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            timePreferenceTable->setItem(row, day + kWeekdayStartColumn, item);
            updateTimePreferenceCell(day, row + 1);
        }
    }

    timePreferenceTable->setStyleSheet(
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

void MainWindow::toggleAvoidTimeSlot(int row, int column)
{
    // 第 0 列是时间说明，不能作为偏好设置。
    if (row < 0 || row >= kPeriodCount || column < kWeekdayStartColumn
        || column >= kColumnCount) {
        return;
    }

    const int day = column - kWeekdayStartColumn;
    const int period = row + 1;

    avoidTimeSlots[day][row] = !avoidTimeSlots[day][row];
    updateTimePreferenceCell(day, period);
    updateTimePreferenceSummary();
}

void MainWindow::updateTimePreferenceCell(int day, int period)
{
    if (timePreferenceTable == nullptr || day < 0 || day >= 7
        || period < 1 || period > kPeriodCount) {
        return;
    }

    QTableWidgetItem* item = timePreferenceTable->item(
        period - 1, day + kWeekdayStartColumn);
    if (item == nullptr) {
        return;
    }

    if (avoidTimeSlots[day][period - 1]) {
        item->setText("尽量避开\n已选择");
        item->setBackground(QColor("#F7C7C7"));
        item->setToolTip("自动排课时应尽量避开这个时间。");
    } else {
        item->setText("可安排");
        item->setBackground(QColor("#EAF4E3"));
        item->setToolTip("点击后标记为尽量避开上课的时间。");
    }
}

void MainWindow::updateTimePreferenceSummary()
{
    if (timePreferenceSummaryLabel == nullptr) {
        return;
    }

    int selectedCount = 0;
    for (const std::array<bool, 13>& periods : avoidTimeSlots) {
        for (bool selected : periods) {
            if (selected) {
                ++selectedCount;
            }
        }
    }

    timePreferenceSummaryLabel->setText(
        "当前已选择 " + QString::number(selectedCount)
        + " 个尽量避开时间。当前版本会保存该偏好，"
          "排课算法接入后将据此尽量避开这些时间。");
}

void MainWindow::loadTimePreferences()
{
    const std::string filePath = projectDirectory() + "/data/time_preference.json";
    std::vector<TimePreferenceBlock> blocks;
    std::string errorMessage;

    if (!TimePreferenceStorage::loadAvoidTimeBlocks(filePath, blocks,
                                                     errorMessage)) {
        statusLabel->setText("读取时间偏好失败："
                             + QString::fromStdString(errorMessage));
        return;
    }

    avoidTimeSlots = {};
    for (const TimePreferenceBlock& block : blocks) {
        if (block.day >= 0 && block.day < 7 && block.beginPeriod >= 1
            && block.beginPeriod <= kPeriodCount) {
            avoidTimeSlots[block.day][block.beginPeriod - 1] = true;
        }
    }

    for (int day = 0; day < 7; ++day) {
        for (int period = 1; period <= kPeriodCount; ++period) {
            updateTimePreferenceCell(day, period);
        }
    }
    updateTimePreferenceSummary();
}

void MainWindow::saveTimePreferences()
{
    std::vector<TimePreferenceBlock> blocks;

    for (int day = 0; day < 7; ++day) {
        for (int period = 1; period <= kPeriodCount; ++period) {
            if (!avoidTimeSlots[day][period - 1]) {
                continue;
            }

            TimePreferenceBlock block;
            block.day = day;
            block.beginPeriod = period;
            block.duration = 1;
            block.hard = false;
            block.reason = "用户设置的尽量避开时间";
            blocks.push_back(block);
        }
    }

    const std::string filePath = projectDirectory() + "/data/time_preference.json";
    std::string errorMessage;

    if (!TimePreferenceStorage::saveAvoidTimeBlocks(filePath, blocks,
                                                     errorMessage)) {
        QMessageBox::critical(this, "保存失败",
                              QString::fromStdString(errorMessage));
        return;
    }

    statusLabel->setText("时间偏好已保存到 data/time_preference.json。");
    QMessageBox::information(this, "保存成功",
                             "已保存尽量避开上课的时间偏好。");
}

void MainWindow::clearTimePreferences()
{
    avoidTimeSlots = {};

    for (int day = 0; day < 7; ++day) {
        for (int period = 1; period <= kPeriodCount; ++period) {
            updateTimePreferenceCell(day, period);
        }
    }

    updateTimePreferenceSummary();
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
    exportButton->setEnabled(false);
    hasScheduleResult = false;
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

    currentScheduleResult = result;
    hasScheduleResult = true;

    showScheduleResult(currentScheduleResult);
    exportButton->setEnabled(true);

    generateButton->setEnabled(true);
}

void MainWindow::exportSchedule()
{
    if (!hasScheduleResult) {
        QMessageBox::information(this, "暂不能导出",
                                 "请先生成一份八学期课程规划。");
        return;
    }

    const QString defaultFilePath = QDir::homePath()
        + "/课程规划结果.csv";
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "导出课程规划 CSV",
        defaultFilePath,
        "CSV 文件 (*.csv)");

    if (filePath.isEmpty()) {
        return;
    }

    if (!filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        filePath += ".csv";
    }

    std::string errorMessage;
    if (!ScheduleExporter::exportToCsv(currentScheduleResult,
                                       filePath.toStdString(),
                                       errorMessage)) {
        QMessageBox::critical(this, "导出失败",
                              QString::fromStdString(errorMessage));
        return;
    }

    statusLabel->setText("课程规划已导出到：" + filePath);
    QMessageBox::information(this, "导出成功",
                             "课程规划 CSV 已成功保存。\n\n"
                             + filePath);
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

QString MainWindow::formatSectionTimeSlots(const CourseSection& section)
{
    const QStringList weekdayNames = {
        "周一", "周二", "周三", "周四", "周五", "周六", "周日"
    };

    QStringList parts;
    for (const TimeSlot& slot : section.timeSlots) {
        if (slot.day < 0 || slot.day >= weekdayNames.size()
            || slot.beginPeriod <= 0 || slot.duration <= 0) {
            continue;
        }

        QString text = weekdayNames[slot.day] + " 第 "
            + QString::number(slot.beginPeriod);
        if (slot.duration > 1) {
            text += "-" + QString::number(slot.endPeriod());
        }
        text += " 节";
        parts.append(text);
    }

    return parts.join("；");
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
