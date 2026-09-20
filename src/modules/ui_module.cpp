#include "BuiltinModules.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "ModernUI.h"
#endif

#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct AppJsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<AppJsonValue> arr;
    std::unordered_map<std::string, AppJsonValue> obj;

    std::string to_string() const {
        if (type == String) return str;
        if (type == Number) {
            std::string s = std::to_string(num);
            if (s.find('.') != std::string::npos) {
                while (s.size() > 1 && s.back() == '0') s.pop_back();
                if (!s.empty() && s.back() == '.') s.pop_back();
            }
            return s;
        }
        if (type == Bool) return b ? "true" : "false";
        return "";
    }

    double to_number(double fallback = 0.0) const {
        if (type == Number) return num;
        if (type == Bool) return b ? 1.0 : 0.0;
        if (type == String) {
            try { return std::stod(str); } catch (...) { return fallback; }
        }
        return fallback;
    }

    int to_int(int fallback = 0) const {
        return static_cast<int>(to_number(fallback));
    }

    bool to_bool(bool fallback = false) const {
        if (type == Bool) return b;
        if (type == Number) return num != 0.0;
        if (type == String) return str == "true" || str == "1";
        return fallback;
    }
};

#ifdef _WIN32
RayQuiroModernUI& runtime() {
    static RayQuiroModernUI ui;
    return ui;
}
#endif

char* duplicate_string(const std::string& value) {
    char* memory = new char[value.size() + 1];
    std::memcpy(memory, value.c_str(), value.size() + 1);
    return memory;
}

void json_skip_ws(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) ++pos;
}

std::string json_parse_string(const std::string& text, size_t& pos) {
    if (pos >= text.size() || text[pos] != '"') throw std::runtime_error("Invalid JSON string.");
    ++pos;
    std::string result;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') return result;
        if (ch == '\\') {
            if (pos >= text.size()) break;
            const char escaped = text[pos++];
            if (escaped == 'n') result += '\n';
            else if (escaped == 'r') result += '\r';
            else if (escaped == 't') result += '\t';
            else result += escaped;
            continue;
        }
        result += ch;
    }
    throw std::runtime_error("Unterminated JSON string.");
}

AppJsonValue json_parse_value(const std::string& text, size_t& pos);

AppJsonValue json_parse_object(const std::string& text, size_t& pos) {
    AppJsonValue val;
    val.type = AppJsonValue::Object;
    ++pos;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == '}') {
        ++pos;
        return val;
    }
    while (pos < text.size()) {
        json_skip_ws(text, pos);
        std::string key = json_parse_string(text, pos);
        json_skip_ws(text, pos);
        if (pos >= text.size() || text[pos] != ':') throw std::runtime_error("Expected ':' in JSON object.");
        ++pos;
        AppJsonValue fieldVal = json_parse_value(text, pos);
        val.obj[key] = fieldVal;
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            break;
        }
        throw std::runtime_error("Invalid JSON object syntax.");
    }
    return val;
}

AppJsonValue json_parse_array(const std::string& text, size_t& pos) {
    AppJsonValue val;
    val.type = AppJsonValue::Array;
    ++pos;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == ']') {
        ++pos;
        return val;
    }
    while (pos < text.size()) {
        val.arr.push_back(json_parse_value(text, pos));
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            break;
        }
        throw std::runtime_error("Invalid JSON array syntax.");
    }
    return val;
}

AppJsonValue json_parse_value(const std::string& text, size_t& pos) {
    json_skip_ws(text, pos);
    if (pos >= text.size()) throw std::runtime_error("Unexpected end of JSON.");
    if (text[pos] == '"') {
        AppJsonValue val;
        val.type = AppJsonValue::String;
        val.str = json_parse_string(text, pos);
        return val;
    }
    if (text[pos] == '{') return json_parse_object(text, pos);
    if (text[pos] == '[') return json_parse_array(text, pos);
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        AppJsonValue val;
        val.type = AppJsonValue::Bool;
        val.b = true;
        return val;
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        AppJsonValue val;
        val.type = AppJsonValue::Bool;
        val.b = false;
        return val;
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        AppJsonValue val;
        val.type = AppJsonValue::Null;
        return val;
    }
    size_t end = pos;
    while (end < text.size()) {
        const char ch = text[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) break;
        ++end;
    }
    if (end == pos) throw std::runtime_error("Invalid JSON scalar.");
    const double number = std::stod(text.substr(pos, end - pos));
    pos = end;
    AppJsonValue val;
    val.type = AppJsonValue::Number;
    val.num = number;
    return val;
}

std::vector<AppJsonValue> parse_args(const char* jsonArgs) {
    const std::string text = jsonArgs ? jsonArgs : "[]";
    size_t pos = 0;
    json_skip_ws(text, pos);
    if (pos >= text.size() || text[pos] != '[') throw std::runtime_error("Native module args must be a JSON array.");
    AppJsonValue arr = json_parse_array(text, pos);
    return arr.arr;
}

std::string dispatch(const std::string& functionName, const std::vector<AppJsonValue>& args) {
#ifdef _WIN32
    if (functionName == "init") {
        runtime().init(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_int(920) : 920, args.size() > 2 ? args[2].to_int(620) : 620);
        return "null";
    }
    if (functionName == "style") { if (!args.empty()) runtime().style(args[0].to_string()); return "null"; }
    if (functionName == "hero") { runtime().hero(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_string() : ""); return "null"; }
    if (functionName == "status") { runtime().status(args.size() > 0 ? args[0].to_string() : "", args.size() > 1 ? args[1].to_string() : ""); return "null"; }
    if (functionName == "info") { runtime().info(args.size() > 0 ? args[0].to_string() : "", args.size() > 1 ? args[1].to_string() : ""); return "null"; }
    if (functionName == "text") { runtime().text(args.size() > 0 ? args[0].to_string() : "", args.size() > 1 ? args[1].to_string() : "body"); return "null"; }
    if (functionName == "button" || functionName == "action") {
        if (args.size() < 2) return "null";
        runtime().action(args[0].to_string(), args[1].to_string(), args.size() > 2 ? args[2].to_string() : "primary");
        return "null";
    }
    if (functionName == "run") {
        return "\"" + runtime().run() + "\"";
    }
#endif
    throw std::runtime_error("rayquiro.ui does not export ui." + functionName);
}

}

int rqm_builtin_ui_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message) {
    try {
        const std::vector<AppJsonValue> args = parse_args(json_args);
        const std::string result = dispatch(function_name == nullptr ? "" : std::string(function_name), args);
        if (json_result != nullptr) *json_result = duplicate_string(result);
        if (error_message != nullptr) *error_message = nullptr;
        return 0;
    } catch (const std::exception& error) {
        if (json_result != nullptr) *json_result = duplicate_string("null");
        if (error_message != nullptr) *error_message = duplicate_string(error.what());
        return 1;
    }
}
