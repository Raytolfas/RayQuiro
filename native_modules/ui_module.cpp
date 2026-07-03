#include "../../include/rayquiro/NativeModuleABI.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "../../include/rayquiro/ModernUI.h"
#endif

#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
struct ScalarValue {
    std::variant<std::monostate, double, std::string, bool> data;
    ScalarValue() : data(std::monostate{}) {}
    ScalarValue(double value) : data(value) {}
    ScalarValue(std::string value) : data(std::move(value)) {}
    ScalarValue(bool value) : data(value) {}
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

ScalarValue json_parse_scalar(const std::string& text, size_t& pos) {
    json_skip_ws(text, pos);
    if (pos >= text.size()) throw std::runtime_error("Unexpected end of JSON.");
    if (text[pos] == '"') return ScalarValue(json_parse_string(text, pos));
    if (text.compare(pos, 4, "true") == 0) { pos += 4; return ScalarValue(true); }
    if (text.compare(pos, 5, "false") == 0) { pos += 5; return ScalarValue(false); }
    if (text.compare(pos, 4, "null") == 0) { pos += 4; return ScalarValue(); }
    size_t end = pos;
    while (end < text.size()) {
        const char ch = text[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) break;
        ++end;
    }
    if (end == pos) throw std::runtime_error("Invalid JSON scalar.");
    const double number = std::stod(text.substr(pos, end - pos));
    pos = end;
    return ScalarValue(number);
}

std::vector<ScalarValue> parse_args(const char* jsonArgs) {
    const std::string text = jsonArgs ? jsonArgs : "[]";
    size_t pos = 0;
    json_skip_ws(text, pos);
    if (pos >= text.size() || text[pos] != '[') throw std::runtime_error("Native module args must be a JSON array.");
    ++pos;
    std::vector<ScalarValue> result;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == ']') { ++pos; return result; }
    while (pos < text.size()) {
        result.push_back(json_parse_scalar(text, pos));
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
        if (pos < text.size() && text[pos] == ']') { ++pos; break; }
        throw std::runtime_error("Invalid JSON array payload.");
    }
    return result;
}

std::string to_string(const ScalarValue& value) {
    if (std::holds_alternative<std::monostate>(value.data)) return "";
    if (std::holds_alternative<std::string>(value.data)) return std::get<std::string>(value.data);
    if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? "true" : "false";
    return std::to_string(std::get<double>(value.data));
}

int to_int(const ScalarValue& value, int fallback = 0) {
    if (std::holds_alternative<double>(value.data)) return static_cast<int>(std::get<double>(value.data));
    if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? 1 : 0;
    if (std::holds_alternative<std::string>(value.data)) {
        try { return std::stoi(std::get<std::string>(value.data)); } catch (...) { return fallback; }
    }
    return fallback;
}

std::string dispatch(const std::string& functionName, const std::vector<ScalarValue>& args) {
#ifdef _WIN32
    if (functionName == "init") {
        runtime().init(args.size() > 0 ? to_string(args[0]) : "RayQuiro", args.size() > 1 ? to_int(args[1], 920) : 920, args.size() > 2 ? to_int(args[2], 620) : 620);
        return "null";
    }
    if (functionName == "style") { if (!args.empty()) runtime().style(to_string(args[0])); return "null"; }
    if (functionName == "hero") { runtime().hero(args.size() > 0 ? to_string(args[0]) : "RayQuiro", args.size() > 1 ? to_string(args[1]) : ""); return "null"; }
    if (functionName == "status") { runtime().status(args.size() > 0 ? to_string(args[0]) : "", args.size() > 1 ? to_string(args[1]) : ""); return "null"; }
    if (functionName == "info") { runtime().info(args.size() > 0 ? to_string(args[0]) : "", args.size() > 1 ? to_string(args[1]) : ""); return "null"; }
    if (functionName == "text") { runtime().text(args.size() > 0 ? to_string(args[0]) : "", args.size() > 1 ? to_string(args[1]) : "body"); return "null"; }
    if (functionName == "button" || functionName == "action") {
        if (args.size() < 2) return "null";
        runtime().action(to_string(args[0]), to_string(args[1]), args.size() > 2 ? to_string(args[2]) : "primary");
        return "null";
    }
    if (functionName == "run") {
        return "\"" + runtime().run() + "\"";
    }
#endif
    throw std::runtime_error("rayquiro.ui.dll does not export ui." + functionName);
}
}

RQM_EXPORT int rqm_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message) {
    try {
        const std::vector<ScalarValue> args = parse_args(json_args);
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

RQM_EXPORT void rqm_free(char* memory) {
    delete[] memory;
}
