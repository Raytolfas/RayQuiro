#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Bytecode.h"

class VM {
public:
    using BuiltinHandler = std::function<std::optional<VMValue>(const std::string&, const std::vector<VMValue>&)>;

    VM() = default;

    void setBuiltinHandler(BuiltinHandler handler) {
        builtinHandler_ = std::move(handler);
    }

    VMValue run(const BytecodeProgram& program) {
        globals_.clear();
        stack_.clear();
        return execute(program, program.entry, {});
    }

private:
    struct Frame {
        const BytecodeFunction* function = nullptr;
        std::size_t ip = 0;
        std::vector<VMValue> locals;
    };

    std::unordered_map<std::string, VMValue> globals_;
    std::vector<VMValue> stack_;
    BuiltinHandler builtinHandler_;

    static bool isTruthy(const VMValue& value) {
        if (std::holds_alternative<std::monostate>(value.data)) return false;
        if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data);
        if (std::holds_alternative<double>(value.data)) return std::get<double>(value.data) != 0.0;
        if (std::holds_alternative<std::string>(value.data)) return !std::get<std::string>(value.data).empty();
        if (std::holds_alternative<VMValue::Array>(value.data)) return !std::get<VMValue::Array>(value.data).empty();
        if (std::holds_alternative<VMValue::Object>(value.data)) return !std::get<VMValue::Object>(value.data).empty();
        return false;
    }

    static double toNumber(const VMValue& value) {
        if (std::holds_alternative<double>(value.data)) return std::get<double>(value.data);
        if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? 1.0 : 0.0;
        if (std::holds_alternative<std::string>(value.data)) {
            try {
                return std::stod(std::get<std::string>(value.data));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }

    static std::string toString(const VMValue& value) {
        if (std::holds_alternative<std::monostate>(value.data)) return "null";
        if (std::holds_alternative<double>(value.data)) {
            std::string raw = std::to_string(std::get<double>(value.data));
            while (raw.size() > 2 && raw.back() == '0') raw.pop_back();
            if (!raw.empty() && raw.back() == '.') raw.pop_back();
            return raw;
        }
        if (std::holds_alternative<std::string>(value.data)) return std::get<std::string>(value.data);
        if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? "true" : "false";
        if (std::holds_alternative<VMValue::Array>(value.data)) {
            const auto& items = std::get<VMValue::Array>(value.data);
            std::string result = "[";
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i) result += ", ";
                result += toString(items[i]);
            }
            result += "]";
            return result;
        }
        if (std::holds_alternative<VMValue::Object>(value.data)) {
            const auto& object = std::get<VMValue::Object>(value.data);
            std::string result = "{";
            bool first = true;
            for (const auto& [key, item] : object) {
                if (!first) result += ", ";
                first = false;
                result += key + ": " + toString(item);
            }
            result += "}";
            return result;
        }
        return "[complex]";
    }

    static int sizeOf(const VMValue& value) {
        if (std::holds_alternative<std::string>(value.data)) return static_cast<int>(std::get<std::string>(value.data).size());
        if (std::holds_alternative<VMValue::Array>(value.data)) return static_cast<int>(std::get<VMValue::Array>(value.data).size());
        if (std::holds_alternative<VMValue::Object>(value.data)) return static_cast<int>(std::get<VMValue::Object>(value.data).size());
        return 0;
    }

    static VMValue indexInto(const VMValue& target, const VMValue& index) {
        const int slot = static_cast<int>(toNumber(index));
        if (std::holds_alternative<std::string>(target.data)) {
            const auto& text = std::get<std::string>(target.data);
            if (slot < 0 || slot >= static_cast<int>(text.size())) return VMValue();
            return VMValue(std::string(1, text[static_cast<std::size_t>(slot)]));
        }
        if (std::holds_alternative<VMValue::Array>(target.data)) {
            const auto& items = std::get<VMValue::Array>(target.data);
            if (slot < 0 || slot >= static_cast<int>(items.size())) return VMValue();
            return items[static_cast<std::size_t>(slot)];
        }
        if (std::holds_alternative<VMValue::Object>(target.data)) {
            const auto& object = std::get<VMValue::Object>(target.data);
            const std::string key = toString(index);
            const auto found = object.find(key);
            if (found != object.end()) {
                return found->second;
            }
            return VMValue();
        }
        return VMValue();
    }

    static std::string jsonEscapeText(const std::string& value) {
        std::string result;
        result.reserve(value.size() + 8);
        for (char ch : value) {
            switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
            }
        }
        return result;
    }

    static std::string jsonStringifyValue(const VMValue& value) {
        if (std::holds_alternative<std::monostate>(value.data)) return "null";
        if (std::holds_alternative<double>(value.data)) return toString(value);
        if (std::holds_alternative<std::string>(value.data)) return "\"" + jsonEscapeText(std::get<std::string>(value.data)) + "\"";
        if (std::holds_alternative<bool>(value.data)) return std::get<bool>(value.data) ? "true" : "false";
        if (std::holds_alternative<VMValue::Array>(value.data)) {
            const auto& items = std::get<VMValue::Array>(value.data);
            std::string result = "[";
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i) result += ",";
                result += jsonStringifyValue(items[i]);
            }
            result += "]";
            return result;
        }
        if (std::holds_alternative<VMValue::Object>(value.data)) {
            const auto& object = std::get<VMValue::Object>(value.data);
            std::string result = "{";
            bool first = true;
            for (const auto& [key, item] : object) {
                if (!first) result += ",";
                first = false;
                result += "\"" + jsonEscapeText(key) + "\":" + jsonStringifyValue(item);
            }
            result += "}";
            return result;
        }
        return "null";
    }

    static void jsonSkipWs(const std::string& text, std::size_t& pos) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
            ++pos;
        }
    }

    static std::string jsonParseString(const std::string& text, std::size_t& pos) {
        if (pos >= text.size() || text[pos] != '"') {
            throw std::runtime_error("Invalid JSON string.");
        }
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

    static VMValue jsonParseValue(const std::string& text, std::size_t& pos) {
        jsonSkipWs(text, pos);
        if (pos >= text.size()) {
            throw std::runtime_error("Unexpected end of JSON input.");
        }

        if (text[pos] == '"') {
            return VMValue(jsonParseString(text, pos));
        }
        if (text[pos] == '[') {
            ++pos;
            VMValue::Array values;
            jsonSkipWs(text, pos);
            if (pos < text.size() && text[pos] == ']') {
                ++pos;
                return VMValue(values);
            }
            while (pos < text.size()) {
                values.push_back(jsonParseValue(text, pos));
                jsonSkipWs(text, pos);
                if (pos < text.size() && text[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text[pos] == ']') {
                    ++pos;
                    return VMValue(values);
                }
                throw std::runtime_error("Invalid JSON array.");
            }
            throw std::runtime_error("Unterminated JSON array.");
        }
        if (text[pos] == '{') {
            ++pos;
            VMValue::Object object;
            jsonSkipWs(text, pos);
            if (pos < text.size() && text[pos] == '}') {
                ++pos;
                return VMValue(object);
            }
            while (pos < text.size()) {
                jsonSkipWs(text, pos);
                const std::string key = jsonParseString(text, pos);
                jsonSkipWs(text, pos);
                if (pos >= text.size() || text[pos] != ':') {
                    throw std::runtime_error("Invalid JSON object.");
                }
                ++pos;
                object[key] = jsonParseValue(text, pos);
                jsonSkipWs(text, pos);
                if (pos < text.size() && text[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text[pos] == '}') {
                    ++pos;
                    return VMValue(object);
                }
                throw std::runtime_error("Invalid JSON object.");
            }
            throw std::runtime_error("Unterminated JSON object.");
        }
        if (text.compare(pos, 4, "true") == 0) {
            pos += 4;
            return VMValue(true);
        }
        if (text.compare(pos, 5, "false") == 0) {
            pos += 5;
            return VMValue(false);
        }
        if (text.compare(pos, 4, "null") == 0) {
            pos += 4;
            return VMValue();
        }

        std::size_t end = pos;
        while (end < text.size()) {
            const char ch = text[end];
            if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) {
                break;
            }
            ++end;
        }
        if (end == pos) {
            throw std::runtime_error("Invalid JSON value.");
        }
        const double number = std::stod(text.substr(pos, end - pos));
        pos = end;
        return VMValue(number);
    }

    VMValue execute(const BytecodeProgram& program, const BytecodeFunction& function, const std::vector<VMValue>& args) {
        Frame frame;
        frame.function = &function;
        frame.locals = args;

        while (frame.ip < frame.function->code.size()) {
            const Instruction instruction = frame.function->code[frame.ip++];
            switch (instruction.op) {
            case OpCode::Constant:
                stack_.push_back(frame.function->constants.at(static_cast<std::size_t>(instruction.a)));
                break;
            case OpCode::Null:
                stack_.push_back(VMValue());
                break;
            case OpCode::True:
                stack_.push_back(VMValue(true));
                break;
            case OpCode::False:
                stack_.push_back(VMValue(false));
                break;
            case OpCode::BuildArray: {
                VMValue::Array items(static_cast<std::size_t>(instruction.a));
                for (std::size_t i = 0; i < items.size(); ++i) {
                    items[items.size() - 1 - i] = pop();
                }
                stack_.push_back(VMValue(items));
                break;
            }
            case OpCode::GetIndex: {
                const VMValue index = pop();
                const VMValue target = pop();
                stack_.push_back(indexInto(target, index));
                break;
            }
            case OpCode::Pop:
                if (!stack_.empty()) stack_.pop_back();
                break;
            case OpCode::DefineGlobal: {
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                globals_[name] = pop();
                break;
            }
            case OpCode::GetGlobal: {
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                const auto found = globals_.find(name);
                if (found == globals_.end()) throw std::runtime_error("Undefined global: " + name);
                stack_.push_back(found->second);
                break;
            }
            case OpCode::SetGlobal: {
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                globals_[name] = peek();
                break;
            }
            case OpCode::GetLocal:
                stack_.push_back(frame.locals.at(static_cast<std::size_t>(instruction.a)));
                break;
            case OpCode::SetLocal:
                frame.locals.at(static_cast<std::size_t>(instruction.a)) = peek();
                break;
            case OpCode::Add: binary([](const VMValue& a, const VMValue& b) {
                    if (std::holds_alternative<double>(a.data) && std::holds_alternative<double>(b.data)) {
                        return VMValue(std::get<double>(a.data) + std::get<double>(b.data));
                    }
                    return VMValue(toString(a) + toString(b));
                }); break;
            case OpCode::Subtract: numeric([](double a, double b) { return a - b; }); break;
            case OpCode::Multiply: numeric([](double a, double b) { return a * b; }); break;
            case OpCode::Divide: numeric([](double a, double b) { return a / b; }); break;
            case OpCode::Modulo: numeric([](double a, double b) { return std::fmod(a, b); }); break;
            case OpCode::Negate: {
                VMValue value = pop();
                stack_.push_back(VMValue(-toNumber(value)));
                break;
            }
            case OpCode::Not: {
                VMValue value = pop();
                stack_.push_back(VMValue(!isTruthy(value)));
                break;
            }
            case OpCode::Equal: compare([](const VMValue& a, const VMValue& b) { return toString(a) == toString(b); }); break;
            case OpCode::NotEqual: compare([](const VMValue& a, const VMValue& b) { return toString(a) != toString(b); }); break;
            case OpCode::Greater: compareNumbers([](double a, double b) { return a > b; }); break;
            case OpCode::GreaterEqual: compareNumbers([](double a, double b) { return a >= b; }); break;
            case OpCode::Less: compareNumbers([](double a, double b) { return a < b; }); break;
            case OpCode::LessEqual: compareNumbers([](double a, double b) { return a <= b; }); break;
            case OpCode::Jump:
                frame.ip = static_cast<std::size_t>(instruction.a);
                break;
            case OpCode::JumpIfFalse:
                if (!isTruthy(peek())) frame.ip = static_cast<std::size_t>(instruction.a);
                break;
            case OpCode::Loop:
                frame.ip = static_cast<std::size_t>(instruction.a);
                break;
            case OpCode::Call: {
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                auto found = program.functions.find(name);
                std::vector<VMValue> callArgs(static_cast<std::size_t>(instruction.b));
                for (std::size_t i = 0; i < callArgs.size(); ++i) {
                    callArgs[callArgs.size() - 1 - i] = pop();
                }
                if (found != program.functions.end()) {
                    stack_.push_back(execute(program, found->second, callArgs));
                    break;
                }
                if (builtinHandler_) {
                    if (const auto result = builtinHandler_(name, callArgs)) {
                        stack_.push_back(*result);
                        break;
                    }
                }
                throw std::runtime_error("Unknown VM function: " + name);
                break;
            }
            case OpCode::Return:
                return stack_.empty() ? VMValue() : pop();
            }
        }

        return VMValue();
    }

    VMValue pop() {
        if (stack_.empty()) return VMValue();
        VMValue value = stack_.back();
        stack_.pop_back();
        return value;
    }

    VMValue peek() const {
        if (stack_.empty()) return VMValue();
        return stack_.back();
    }

    template <typename Fn>
    void numeric(Fn&& fn) {
        const VMValue right = pop();
        const VMValue left = pop();
        stack_.push_back(VMValue(fn(toNumber(left), toNumber(right))));
    }

    template <typename Fn>
    void compare(Fn&& fn) {
        const VMValue right = pop();
        const VMValue left = pop();
        stack_.push_back(VMValue(fn(left, right)));
    }

    template <typename Fn>
    void compareNumbers(Fn&& fn) {
        const VMValue right = pop();
        const VMValue left = pop();
        stack_.push_back(VMValue(fn(toNumber(left), toNumber(right))));
    }

    template <typename Fn>
    void binary(Fn&& fn) {
        const VMValue right = pop();
        const VMValue left = pop();
        stack_.push_back(fn(left, right));
    }

public:
    static std::optional<VMValue> callDefaultBuiltin(const std::string& name, const std::vector<VMValue>& args) {
        if (name == "print") {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) std::cout << " ";
                std::cout << toString(args[i]);
            }
            std::cout << std::endl;
            return VMValue();
        }
        if (name == "str") return VMValue(args.empty() ? "" : toString(args[0]));
        if (name == "num") return VMValue(args.empty() ? 0.0 : toNumber(args[0]));
        if (name == "bool") return VMValue(!args.empty() && isTruthy(args[0]));
        if (name == "type") {
            if (args.empty() || std::holds_alternative<std::monostate>(args[0].data)) return VMValue("null");
            if (std::holds_alternative<double>(args[0].data)) return VMValue("number");
            if (std::holds_alternative<std::string>(args[0].data)) return VMValue("string");
            if (std::holds_alternative<bool>(args[0].data)) return VMValue("bool");
            if (std::holds_alternative<VMValue::Array>(args[0].data)) return VMValue("array");
            if (std::holds_alternative<VMValue::Object>(args[0].data)) return VMValue("object");
            return VMValue("unknown");
        }
        if (name == "len") {
            if (args.empty()) return VMValue(0.0);
            if (std::holds_alternative<std::string>(args[0].data)) return VMValue(static_cast<double>(std::get<std::string>(args[0].data).size()));
            if (std::holds_alternative<VMValue::Array>(args[0].data)) return VMValue(static_cast<double>(std::get<VMValue::Array>(args[0].data).size()));
            if (std::holds_alternative<VMValue::Object>(args[0].data)) return VMValue(static_cast<double>(std::get<VMValue::Object>(args[0].data).size()));
            return VMValue(0.0);
        }
        if (name == "range") {
            double start = 0.0;
            double end = 0.0;
            double step = 1.0;
            if (args.size() == 1) {
                end = toNumber(args[0]);
            } else if (args.size() >= 2) {
                start = toNumber(args[0]);
                end = toNumber(args[1]);
                if (args.size() >= 3) step = toNumber(args[2]);
            }
            if (step == 0.0) return VMValue(VMValue::Array{});
            VMValue::Array result;
            if (step > 0.0) for (double value = start; value < end; value += step) result.push_back(VMValue(value));
            else for (double value = start; value > end; value += step) result.push_back(VMValue(value));
            return VMValue(result);
        }
        if (name == "push") {
            if (args.size() < 2 || !std::holds_alternative<VMValue::Array>(args[0].data)) return VMValue();
            VMValue::Array values = std::get<VMValue::Array>(args[0].data);
            values.push_back(args[1]);
            return VMValue(values);
        }
        if (name == "pop") {
            if (args.empty() || !std::holds_alternative<VMValue::Array>(args[0].data)) return VMValue();
            const auto& values = std::get<VMValue::Array>(args[0].data);
            if (values.empty()) return VMValue();
            return values.back();
        }
        if (name == "join") {
            if (args.empty() || !std::holds_alternative<VMValue::Array>(args[0].data)) return VMValue("");
            const std::string separator = args.size() > 1 ? toString(args[1]) : "";
            const auto& values = std::get<VMValue::Array>(args[0].data);
            std::string result;
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i) result += separator;
                result += toString(values[i]);
            }
            return VMValue(result);
        }
        if (name == "split") {
            if (args.empty()) return VMValue(VMValue::Array{});
            const std::string text = toString(args[0]);
            const std::string separator = args.size() > 1 ? toString(args[1]) : "";
            VMValue::Array result;
            if (separator.empty()) {
                for (char ch : text) result.push_back(VMValue(std::string(1, ch)));
                return VMValue(result);
            }
            std::size_t start = 0;
            while (true) {
                const std::size_t found = text.find(separator, start);
                if (found == std::string::npos) {
                    result.push_back(VMValue(text.substr(start)));
                    break;
                }
                result.push_back(VMValue(text.substr(start, found - start)));
                start = found + separator.size();
            }
            return VMValue(result);
        }
        if (name == "upper") {
            std::string value = args.empty() ? "" : toString(args[0]);
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return VMValue(value);
        }
        if (name == "lower") {
            std::string value = args.empty() ? "" : toString(args[0]);
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return VMValue(value);
        }
        if (name == "contains") {
            if (args.size() < 2) return VMValue(false);
            if (std::holds_alternative<std::string>(args[0].data)) {
                return VMValue(std::get<std::string>(args[0].data).find(toString(args[1])) != std::string::npos);
            }
            if (std::holds_alternative<VMValue::Array>(args[0].data)) {
                const auto& values = std::get<VMValue::Array>(args[0].data);
                for (const auto& value : values) {
                    if (toString(value) == toString(args[1])) return VMValue(true);
                }
            }
            return VMValue(false);
        }
        if (name == "trim") {
            std::string value = args.empty() ? "" : toString(args[0]);
            const auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
            const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
            return VMValue(start >= end ? "" : std::string(start, end));
        }
        if (name == "replace") {
            if (args.size() < 3) return VMValue(args.empty() ? "" : toString(args[0]));
            std::string text = toString(args[0]);
            const std::string needle = toString(args[1]);
            const std::string replacement = toString(args[2]);
            if (needle.empty()) return VMValue(text);
            size_t start = 0;
            while ((start = text.find(needle, start)) != std::string::npos) {
                text.replace(start, needle.size(), replacement);
                start += replacement.size();
            }
            return VMValue(text);
        }
        if (name == "slice") {
            if (args.empty()) return VMValue();
            const VMValue& source = args[0];
            const int start = args.size() > 1 ? static_cast<int>(toNumber(args[1])) : 0;
            const int end = args.size() > 2 ? static_cast<int>(toNumber(args[2])) : sizeOf(source);
            if (std::holds_alternative<std::string>(source.data)) {
                const auto& text = std::get<std::string>(source.data);
                const int safeStart = std::max(0, start);
                const int safeEnd = std::max(safeStart, std::min(static_cast<int>(text.size()), end));
                return VMValue(text.substr(static_cast<std::size_t>(safeStart), static_cast<std::size_t>(safeEnd - safeStart)));
            }
            if (std::holds_alternative<VMValue::Array>(source.data)) {
                const auto& values = std::get<VMValue::Array>(source.data);
                const int safeStart = std::max(0, start);
                const int safeEnd = std::max(safeStart, std::min(static_cast<int>(values.size()), end));
                VMValue::Array result;
                for (int i = safeStart; i < safeEnd; ++i) result.push_back(values[static_cast<std::size_t>(i)]);
                return VMValue(result);
            }
            return VMValue();
        }
        if (name == "floor") return VMValue(args.empty() ? 0.0 : std::floor(toNumber(args[0])));
        if (name == "ceil") return VMValue(args.empty() ? 0.0 : std::ceil(toNumber(args[0])));
        if (name == "round") return VMValue(args.empty() ? 0.0 : std::round(toNumber(args[0])));
        if (name == "min") {
            if (args.empty()) return VMValue(0.0);
            double result = toNumber(args[0]);
            for (std::size_t i = 1; i < args.size(); ++i) result = std::min(result, toNumber(args[i]));
            return VMValue(result);
        }
        if (name == "max") {
            if (args.empty()) return VMValue(0.0);
            double result = toNumber(args[0]);
            for (std::size_t i = 1; i < args.size(); ++i) result = std::max(result, toNumber(args[i]));
            return VMValue(result);
        }
        if (name == "clamp") {
            if (args.empty()) return VMValue(0.0);
            const double value = toNumber(args[0]);
            const double minValue = args.size() > 1 ? toNumber(args[1]) : 0.0;
            const double maxValue = args.size() > 2 ? toNumber(args[2]) : minValue;
            return VMValue(std::max(minValue, std::min(value, maxValue)));
        }
        if (name == "sleep" || name == "time.sleep") {
            const int duration = args.empty() ? 0 : static_cast<int>(toNumber(args[0]));
            std::this_thread::sleep_for(std::chrono::milliseconds(duration));
            return VMValue();
        }
        if (name == "clock.ms" || name == "time.now_ms") {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            return VMValue(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
        }
        if (name == "time.unix_ms") {
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            return VMValue(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
        }
        if (name == "json.stringify") {
            if (args.empty()) return VMValue("null");
            return VMValue(jsonStringifyValue(args[0]));
        }
        if (name == "json.parse") {
            if (args.empty()) return VMValue();
            const std::string text = toString(args[0]);
            std::size_t pos = 0;
            VMValue result = jsonParseValue(text, pos);
            jsonSkipWs(text, pos);
            if (pos != text.size()) {
                throw std::runtime_error("Unexpected trailing characters in JSON.");
            }
            return result;
        }
        if (name == "random" || name == "random.int") {
            static std::mt19937 engine(static_cast<unsigned int>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()));
            if (name == "random.int") {
                int min = 0;
                int max = 100;
                if (args.size() == 1) max = static_cast<int>(toNumber(args[0]));
                else if (args.size() >= 2) {
                    min = static_cast<int>(toNumber(args[0]));
                    max = static_cast<int>(toNumber(args[1]));
                }
                if (max < min) std::swap(max, min);
                std::uniform_int_distribution<int> distribution(min, max);
                return VMValue(static_cast<double>(distribution(engine)));
            }
            double min = 0.0;
            double max = 1.0;
            if (args.size() == 1) max = toNumber(args[0]);
            else if (args.size() >= 2) {
                min = toNumber(args[0]);
                max = toNumber(args[1]);
            }
            if (max < min) std::swap(max, min);
            std::uniform_real_distribution<double> distribution(min, max);
            return VMValue(distribution(engine));
        }
        return std::nullopt;
    }
};
