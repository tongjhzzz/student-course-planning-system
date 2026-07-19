#include "time_preference_storage.h"

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
} // namespace

std::string TimePreferenceStorage::dayToText(int day)
{
    const char* const weekdayNames[] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };

    if (day < 0 || day > 6) {
        return "";
    }

    return weekdayNames[day];
}

int TimePreferenceStorage::textToDay(const QString& dayText)
{
    const QStringList weekdayNames = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };

    return weekdayNames.indexOf(dayText);
}

bool TimePreferenceStorage::loadAvoidTimeBlocks(
    const std::string& filePath,
    std::vector<TimePreferenceBlock>& avoidTimeBlocks,
    std::string& errorMessage)
{
    QFile inputFile(QString::fromUtf8(filePath.c_str()));

    // 第一次使用时文件还没有创建，这不是错误。
    if (!inputFile.exists()) {
        avoidTimeBlocks.clear();
        errorMessage.clear();
        return true;
    }

    if (!inputFile.open(QIODevice::ReadOnly)) {
        errorMessage = "无法打开时间偏好文件：" + filePath;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        inputFile.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorMessage = "时间偏好 JSON 格式错误。";
        return false;
    }

    const QJsonValue blocksValue =
        document.object().value("avoid_time_blocks");

    if (!blocksValue.isArray()) {
        errorMessage = "时间偏好文件中缺少 avoid_time_blocks 数组。";
        return false;
    }

    std::vector<TimePreferenceBlock> loadedBlocks;
    for (const QJsonValue& value : blocksValue.toArray()) {
        if (!value.isObject()) {
            errorMessage = "avoid_time_blocks 中存在非对象内容。";
            return false;
        }

        const QJsonObject object = value.toObject();
        const int day = textToDay(object.value("day").toString());
        const QJsonValue beginValue = object.value("beg");
        const QJsonValue durationValue = object.value("last");

        if (day < 0 || !beginValue.isDouble() || !durationValue.isDouble()) {
            errorMessage = "时间偏好文件中存在格式错误的时间段。";
            return false;
        }

        TimePreferenceBlock block;
        block.day = day;
        block.beginPeriod = beginValue.toInt();
        block.duration = durationValue.toInt();
        block.hard = false;
        block.reason = toStdString(object.value("reason").toString());

        if (block.beginPeriod < 1 || block.beginPeriod > 13
            || block.duration != 1) {
            errorMessage = "时间偏好中的节次必须是第 1 到 13 节。";
            return false;
        }

        loadedBlocks.push_back(block);
    }

    avoidTimeBlocks = loadedBlocks;
    errorMessage.clear();
    return true;
}

bool TimePreferenceStorage::saveAvoidTimeBlocks(
    const std::string& filePath,
    const std::vector<TimePreferenceBlock>& avoidTimeBlocks,
    std::string& errorMessage)
{
    QJsonArray blocksArray;

    for (const TimePreferenceBlock& block : avoidTimeBlocks) {
        const std::string dayText = dayToText(block.day);

        if (dayText.empty() || block.beginPeriod < 1
            || block.beginPeriod > 13 || block.duration != 1) {
            errorMessage = "存在无法保存的时间偏好数据。";
            return false;
        }

        QJsonObject blockObject;
        blockObject.insert("day", QString::fromStdString(dayText));
        blockObject.insert("beg", block.beginPeriod);
        blockObject.insert("last", block.duration);
        blockObject.insert("hard", false);
        blockObject.insert("reason", "用户设置的尽量避开时间");
        blocksArray.append(blockObject);
    }

    QJsonObject rootObject;
    rootObject.insert("description", "用户设置的尽量避开上课时间");
    rootObject.insert("avoid_time_blocks", blocksArray);

    QFile outputFile(QString::fromUtf8(filePath.c_str()));
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMessage = "无法保存时间偏好文件：" + filePath;
        return false;
    }

    const QJsonDocument document(rootObject);
    if (outputFile.write(document.toJson(QJsonDocument::Indented)) == -1) {
        errorMessage = "写入时间偏好文件时发生错误：" + filePath;
        return false;
    }

    errorMessage.clear();
    return true;
}
