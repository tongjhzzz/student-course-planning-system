#include "constraints_loader.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

// ---------- 极简 JSON 解析 ----------
// 只支持本项目约束文件用到的 JSON 子集：
// 对象、数组、字符串、数字、true/false/null。
// 输入必须是合法的 JSON；遇到非法内容时解析失败并给出位置。

struct JsonValue
{
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::unordered_map<std::string, JsonValue> objectValue;

    // 对象取值；键不存在或本身不是对象时返回 nullptr。
    const JsonValue* find(const std::string& key) const
    {
        if (type != Type::Object) {
            return nullptr;
        }
        auto it = objectValue.find(key);
        return it == objectValue.end() ? nullptr : &it->second;
    }
};

class JsonParser
{
public:
    JsonParser(const std::string& text, std::string& errorMessage)
        : text(text), errorMessage(errorMessage)
    {
    }

    bool parse(JsonValue& result)
    {
        if (!parseValue(result)) {
            return false;
        }
        skipWhitespace();
        if (pos != text.size()) {
            return fail("JSON 末尾存在多余内容");
        }
        return true;
    }

private:
    const std::string& text;
    std::string& errorMessage;
    std::size_t pos = 0;

    bool fail(const std::string& reason)
    {
        std::ostringstream stream;
        stream << reason << "（第 " << (pos + 1) << " 个字符附近）";
        errorMessage = stream.str();
        return false;
    }

    void skipWhitespace()
    {
        while (pos < text.size()
               && (text[pos] == ' ' || text[pos] == '\t'
                   || text[pos] == '\r' || text[pos] == '\n')) {
            ++pos;
        }
    }

    bool parseValue(JsonValue& result)
    {
        skipWhitespace();
        if (pos >= text.size()) {
            return fail("JSON 内容意外结束");
        }

        switch (text[pos]) {
        case '{': return parseObject(result);
        case '[': return parseArray(result);
        case '"': result.type = JsonValue::Type::String;
                  return parseString(result.stringValue);
        case 't': return parseLiteral("true", JsonValue::Type::Bool, result, true);
        case 'f': return parseLiteral("false", JsonValue::Type::Bool, result, false);
        case 'n': return parseLiteral("null", JsonValue::Type::Null, result, false);
        default:  return parseNumber(result);
        }
    }

    bool parseObject(JsonValue& result)
    {
        result.type = JsonValue::Type::Object;
        ++pos; // 跳过 '{'
        skipWhitespace();
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            return true;
        }

        while (true) {
            skipWhitespace();
            if (pos >= text.size() || text[pos] != '"') {
                return fail("对象的键必须是字符串");
            }
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipWhitespace();
            if (pos >= text.size() || text[pos] != ':') {
                return fail("对象的键后面缺少 ':'");
            }
            ++pos;
            if (!parseValue(result.objectValue[key])) {
                return false;
            }
            skipWhitespace();
            if (pos >= text.size()) {
                return fail("对象没有正常结束");
            }
            if (text[pos] == ',') {
                ++pos;
                continue;
            }
            if (text[pos] == '}') {
                ++pos;
                return true;
            }
            return fail("对象中缺少 ',' 或 '}'");
        }
    }

    bool parseArray(JsonValue& result)
    {
        result.type = JsonValue::Type::Array;
        ++pos; // 跳过 '['
        skipWhitespace();
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            return true;
        }

        while (true) {
            JsonValue element;
            if (!parseValue(element)) {
                return false;
            }
            result.arrayValue.push_back(std::move(element));
            skipWhitespace();
            if (pos >= text.size()) {
                return fail("数组没有正常结束");
            }
            if (text[pos] == ',') {
                ++pos;
                continue;
            }
            if (text[pos] == ']') {
                ++pos;
                return true;
            }
            return fail("数组中缺少 ',' 或 ']'");
        }
    }

    bool parseString(std::string& result)
    {
        ++pos; // 跳过开头的 '"'
        result.clear();
        while (pos < text.size()) {
            char ch = text[pos++];
            if (ch == '"') {
                return true;
            }
            if (ch == '\\') {
                if (pos >= text.size()) {
                    break;
                }
                char escaped = text[pos++];
                switch (escaped) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u':
                    // 本项目需要的字段都是 ASCII，\u 转义统一用 '?' 代替。
                    if (pos + 4 > text.size()) {
                        return fail("\\u 转义不完整");
                    }
                    pos += 4;
                    result += '?';
                    break;
                default:
                    return fail("无法识别的转义字符");
                }
            } else {
                result += ch; // UTF-8 中文字符按字节原样保留
            }
        }
        return fail("字符串没有正常结束");
    }

    bool parseNumber(JsonValue& result)
    {
        const std::size_t begin = pos;
        if (pos < text.size() && text[pos] == '-') {
            ++pos;
        }
        while (pos < text.size()
               && (std::isdigit(static_cast<unsigned char>(text[pos]))
                   || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E'
                   || text[pos] == '+' || text[pos] == '-')) {
            ++pos;
        }
        if (begin == pos) {
            return fail("无法识别的 JSON 值");
        }
        result.type = JsonValue::Type::Number;
        result.numberValue = std::strtod(text.substr(begin, pos - begin).c_str(), nullptr);
        return true;
    }

    bool parseLiteral(const char* literal, JsonValue::Type type,
                      JsonValue& result, bool boolValue)
    {
        const std::size_t length = std::string(literal).size();
        if (text.compare(pos, length, literal) != 0) {
            return fail("无法识别的 JSON 值");
        }
        pos += length;
        result.type = type;
        result.boolValue = boolValue;
        return true;
    }
};

// ---------- 约束字段提取 ----------

bool readIdArray(const JsonValue& root, const char* key,
                 std::vector<std::string>& ids, std::string& errorMessage)
{
    const JsonValue* scope = root.find("target_course_scope");
    const JsonValue* array = scope == nullptr ? nullptr : scope->find(key);
    if (array == nullptr || array->type != JsonValue::Type::Array) {
        errorMessage = std::string("约束文件缺少数组字段 target_course_scope.") + key;
        return false;
    }
    for (const JsonValue& element : array->arrayValue) {
        if (element.type == JsonValue::Type::String) {
            ids.push_back(element.stringValue);
        }
    }
    return true;
}

bool readNumber(const JsonValue& root, const char* key,
                double& value, std::string& errorMessage)
{
    const JsonValue* number = root.find(key);
    if (number == nullptr || number->type != JsonValue::Type::Number) {
        errorMessage = std::string("约束文件缺少数字字段 ") + key;
        return false;
    }
    value = number->numberValue;
    return true;
}

} // namespace

bool ConstraintsLoader::loadFromJsonFile(const std::string& filePath,
                                         ScheduleConstraints& constraints,
                                         std::string& errorMessage)
{
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        errorMessage = "无法打开约束文件 " + filePath;
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string text = buffer.str();

    // 去掉可能存在的 UTF-8 BOM，避免解析失败。
    if (text.size() >= 3
        && text[0] == '\xEF' && text[1] == '\xBB' && text[2] == '\xBF') {
        text.erase(0, 3);
    }

    JsonValue root;
    JsonParser parser(text, errorMessage);
    if (!parser.parse(root)) {
        return false;
    }

    if (!readIdArray(root, "required_course_basic_IDs",
                     constraints.requiredCourseIds, errorMessage)) {
        return false;
    }
    if (!readIdArray(root, "elective_candidate_course_basic_IDs",
                     constraints.electiveCandidateIds, errorMessage)) {
        return false;
    }

    const JsonValue* caps = root.find("max_credit_per_semester");
    if (caps == nullptr || caps->type != JsonValue::Type::Object) {
        errorMessage = "约束文件缺少对象字段 max_credit_per_semester";
        return false;
    }
    for (int term = 1; term <= 8; ++term) {
        const JsonValue* cap = caps->find(std::to_string(term));
        if (cap == nullptr || cap->type != JsonValue::Type::Number) {
            errorMessage = "max_credit_per_semester 缺少学期 " + std::to_string(term);
            return false;
        }
        constraints.maxCreditPerTerm[term] = cap->numberValue;
    }

    // 每学期学分下限是可选字段；缺省时保持 0，表示不做要求。
    const JsonValue* mins = root.find("min_credit_per_semester");
    if (mins != nullptr) {
        if (mins->type != JsonValue::Type::Object) {
            errorMessage = "min_credit_per_semester 必须是对象";
            return false;
        }
        for (int term = 1; term <= 8; ++term) {
            const JsonValue* min = mins->find(std::to_string(term));
            if (min == nullptr || min->type != JsonValue::Type::Number) {
                errorMessage = "min_credit_per_semester 缺少学期 " + std::to_string(term);
                return false;
            }
            constraints.minCreditPerTerm[term] = min->numberValue;
        }
    }

    if (!readNumber(root, "min_total_credit",
                    constraints.minTotalCredit, errorMessage)) {
        return false;
    }
    if (!readNumber(root, "elective_min_credit",
                    constraints.electiveMinCredit, errorMessage)) {
        return false;
    }

    return true;
}
