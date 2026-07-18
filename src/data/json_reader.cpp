#include "json_reader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

namespace
{
std::string toStdString(const QString& text)
{
    return text.toUtf8().toStdString();
}

int dayToNumber(const QString& dayText)
{
    if (dayText == "Mon") {
        return 0;
    }
    if (dayText == "Tue") {
        return 1;
    }
    if (dayText == "Wed") {
        return 2;
    }
    if (dayText == "Thu") {
        return 3;
    }
    if (dayText == "Fri") {
        return 4;
    }
    if (dayText == "Sat") {
        return 5;
    }
    if (dayText == "Sun") {
        return 6;
    }

    return -1;
}

bool readStringArray(const QJsonObject& object,
                     const QString& key,
                     std::vector<std::string>& result,
                     std::string& errorMessage)
{
    const QJsonValue value = object.value(key);

    if (!value.isArray()) {
        errorMessage = "JSON 中缺少字符串数组字段：" + toStdString(key);
        return false;
    }

    result.clear();
    const QJsonArray array = value.toArray();

    for (const QJsonValue& item : array) {
        if (!item.isString()) {
            errorMessage = "字段 " + toStdString(key) + " 中存在非字符串内容。";
            return false;
        }

        result.push_back(toStdString(item.toString()));
    }

    return true;
}

bool readCreditPerSemester(const QJsonObject& root,
                           const QString& key,
                           std::array<double, 8>& credits,
                           bool required,
                           std::string& errorMessage)
{
    const QJsonValue value = root.value(key);

    if (value.isUndefined()) {
        if (required) {
            errorMessage = "JSON 中缺少学期学分字段：" + toStdString(key);
            return false;
        }

        return true;
    }

    if (!value.isObject()) {
        errorMessage = "字段 " + toStdString(key) + " 应为对象。";
        return false;
    }

    const QJsonObject creditObject = value.toObject();

    for (int term = 1; term <= 8; ++term) {
        const QString termKey = QString::number(term);
        const QJsonValue creditValue = creditObject.value(termKey);

        if (!creditValue.isDouble()) {
            errorMessage = "字段 " + toStdString(key)
                + " 中缺少第 " + std::to_string(term) + " 学期的数值。";
            return false;
        }

        credits[static_cast<std::size_t>(term - 1)] = creditValue.toDouble();
    }

    return true;
}

bool readTimeBlocks(const QJsonObject& root,
                    const QString& key,
                    std::vector<TimePreferenceBlock>& result,
                    std::string& errorMessage)
{
    const QJsonValue value = root.value(key);

    // 时间偏好是可选项。培养方案没有提供时，保留空列表即可。
    if (value.isUndefined()) {
        result.clear();
        return true;
    }

    if (!value.isArray()) {
        errorMessage = "字段 " + toStdString(key) + " 应为数组。";
        return false;
    }

    result.clear();

    for (const QJsonValue& item : value.toArray()) {
        if (!item.isObject()) {
            errorMessage = "字段 " + toStdString(key) + " 中存在非对象内容。";
            return false;
        }

        const QJsonObject blockObject = item.toObject();
        const int day = dayToNumber(blockObject.value("day").toString());
        const QJsonValue beginValue = blockObject.value("beg");
        const QJsonValue durationValue = blockObject.value("last");

        if (day == -1 || !beginValue.isDouble() || !durationValue.isDouble()) {
            errorMessage = "字段 " + toStdString(key)
                + " 中存在格式错误的时间段。";
            return false;
        }

        TimePreferenceBlock block;
        block.day = day;
        block.beginPeriod = beginValue.toInt();
        block.duration = durationValue.toInt();
        block.hard = blockObject.value("hard").toBool(false);
        block.reason = toStdString(blockObject.value("reason").toString());

        result.push_back(block);
    }

    return true;
}
} // namespace

bool JsonReader::loadPlanningConstraints(const std::string& filePath,
                                         PlanningConstraints& constraints,
                                         std::string& errorMessage)
{
    QFile file(QString::fromUtf8(filePath.c_str()));

    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = "无法打开 JSON 文件：" + filePath;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = "JSON 格式错误：" + toStdString(parseError.errorString());
        return false;
    }

    if (!document.isObject()) {
        errorMessage = "JSON 根节点应为对象。";
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonValue courseScopeValue = root.value("target_course_scope");

    if (!courseScopeValue.isObject()) {
        errorMessage = "JSON 中缺少 target_course_scope 对象。";
        return false;
    }

    const QJsonObject courseScope = courseScopeValue.toObject();

    PlanningConstraints loadedConstraints;
    loadedConstraints.profileId = toStdString(root.value("profile_id").toString());
    loadedConstraints.profileName = toStdString(root.value("profile_name").toString());
    loadedConstraints.description = toStdString(root.value("description").toString());
    loadedConstraints.targetDepartment =
        toStdString(root.value("target_department").toString());

    if (!readStringArray(root, "support_departments",
                         loadedConstraints.supportDepartments, errorMessage)
        || !readStringArray(courseScope, "required_course_basic_IDs",
                            loadedConstraints.requiredCourseIds, errorMessage)
        || !readStringArray(courseScope, "elective_candidate_course_basic_IDs",
                            loadedConstraints.electiveCandidateCourseIds, errorMessage)
        || !readCreditPerSemester(root, "min_credit_per_semester",
                                  loadedConstraints.minCreditPerSemester,
                                  false, errorMessage)
        || !readCreditPerSemester(root, "max_credit_per_semester",
                                  loadedConstraints.maxCreditPerSemester,
                                  true, errorMessage)
        || !readTimeBlocks(root, "avoid_time_blocks",
                           loadedConstraints.avoidTimeBlocks, errorMessage)
        || !readTimeBlocks(root, "preferred_time_blocks",
                           loadedConstraints.preferredTimeBlocks, errorMessage)) {
        return false;
    }

    const QJsonValue minTotalCreditValue = root.value("min_total_credit");
    const QJsonValue electiveMinCreditValue = root.value("elective_min_credit");
    const QJsonValue requiredRuleValue = root.value("required_rule");

    if (!minTotalCreditValue.isDouble() || !electiveMinCreditValue.isDouble()
        || !requiredRuleValue.isObject()) {
        errorMessage = "JSON 中缺少必要的总学分、选修学分或必修规则字段。";
        return false;
    }

    const QJsonObject requiredRule = requiredRuleValue.toObject();
    const QJsonValue requiredCreditValue = requiredRule.value("required_credit");

    if (!requiredCreditValue.isDouble()) {
        errorMessage = "required_rule 中缺少 required_credit 数值。";
        return false;
    }

    loadedConstraints.minTotalCredit = minTotalCreditValue.toDouble();
    loadedConstraints.requiredCredit = requiredCreditValue.toDouble();
    loadedConstraints.electiveMinCredit = electiveMinCreditValue.toDouble();

    constraints = loadedConstraints;
    errorMessage.clear();
    return true;
}
