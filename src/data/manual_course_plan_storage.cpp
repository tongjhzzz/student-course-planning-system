#include "manual_course_plan_storage.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

bool ManualCoursePlanStorage::loadSelections(
    const std::string& filePath,
    std::vector<ManualCourseSelection>& selections,
    std::string& errorMessage)
{
    QFile inputFile(QString::fromUtf8(filePath.c_str()));

    // 第一次使用时文件还不存在，这代表用户还没有保存过方案
    if (!inputFile.exists()) {
        selections.clear();
        errorMessage.clear();
        return true;
    }

    if (!inputFile.open(QIODevice::ReadOnly)) {
        errorMessage = "无法打开手动选课方案文件：" + filePath;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        inputFile.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorMessage = "手动选课方案 JSON 格式错误。";
        return false;
    }

    const QJsonValue selectionsValue =
        document.object().value("manual_course_selections");

    if (!selectionsValue.isArray()) {
        errorMessage = "手动选课方案中缺少 manual_course_selections 数组。";
        return false;
    }

    std::vector<ManualCourseSelection> loadedSelections;
    for (const QJsonValue& value : selectionsValue.toArray()) {
        if (!value.isObject()) {
            errorMessage = "manual_course_selections 中存在非对象内容。";
            return false;
        }

        const QJsonObject object = value.toObject();
        const QString basicId = object.value("course_basic_ID").toString();
        const QString sectionId = object.value("course_sp_ID").toString();
        const QJsonValue termValue = object.value("term");

        if (basicId.isEmpty() || sectionId.isEmpty() || !termValue.isDouble()) {
            errorMessage = "手动选课方案中存在格式错误的课程记录。";
            return false;
        }

        ManualCourseSelection selection;
        selection.basicId = basicId.toUtf8().toStdString();
        selection.sectionId = sectionId.toUtf8().toStdString();
        selection.term = termValue.toInt();

        if (selection.term < 1 || selection.term > 8) {
            errorMessage = "手动选课方案中的学期必须在第 1 到第 8 学期之间。";
            return false;
        }

        loadedSelections.push_back(selection);
    }

    selections = loadedSelections;
    errorMessage.clear();
    return true;
}

bool ManualCoursePlanStorage::saveSelections(
    const std::string& filePath,
    const std::vector<ManualCourseSelection>& selections,
    std::string& errorMessage)
{
    QJsonArray selectionsArray;

    for (const ManualCourseSelection& selection : selections) {
        if (selection.basicId.empty() || selection.sectionId.empty()
            || selection.term < 1 || selection.term > 8) {
            errorMessage = "存在无法保存的手动选课记录。";
            return false;
        }

        QJsonObject selectionObject;
        selectionObject.insert("course_basic_ID",
                               QString::fromStdString(selection.basicId));
        selectionObject.insert("course_sp_ID",
                               QString::fromStdString(selection.sectionId));
        selectionObject.insert("term", selection.term);
        selectionsArray.append(selectionObject);
    }

    QJsonObject rootObject;
    rootObject.insert("description", "用户手动选择的课程方案");
    rootObject.insert("manual_course_selections", selectionsArray);

    QFile outputFile(QString::fromUtf8(filePath.c_str()));
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMessage = "无法保存手动选课方案文件：" + filePath;
        return false;
    }

    const QJsonDocument document(rootObject);
    if (outputFile.write(document.toJson(QJsonDocument::Indented)) == -1) {
        errorMessage = "写入手动选课方案时发生错误：" + filePath;
        return false;
    }

    errorMessage.clear();
    return true;
}
