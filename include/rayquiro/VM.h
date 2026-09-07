#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <ctime>
#include <filesystem>
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
        globals_["pi"] = VMValue(3.14159265358979323846);
        stack_.clear();
        return execute(program, program.entry, {});
    }

private:
    struct CatchFrame {
        std::size_t catch_addr;
        std::string error_var; // global var name to store error message
    };

    struct Frame {
        const BytecodeFunction* function = nullptr;
        std::size_t ip = 0;
        std::vector<VMValue> locals;
        std::vector<CatchFrame> catchStack; // active try/catch frames
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

    static bool isNull(const VMValue& value) {
        return std::holds_alternative<std::monostate>(value.data);
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
        // Pre-size locals: enough for params + all declared locals
        int totalSlots = std::max(static_cast<int>(args.size()), function.localCount);
        frame.locals.resize(static_cast<std::size_t>(totalSlots));
        for (std::size_t i = 0; i < args.size(); ++i) {
            frame.locals[i] = args[i];
        }

        while (frame.ip < frame.function->code.size()) {
        try { // try/catch wrapper for TryCatch support
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
            case OpCode::BuildObject: {
                // instruction.a = number of key-value pairs
                VMValue::Object obj;
                std::vector<std::pair<std::string, VMValue>> pairs(static_cast<std::size_t>(instruction.a));
                for (int i = static_cast<int>(pairs.size()) - 1; i >= 0; --i) {
                    VMValue val = pop();
                    VMValue key = pop();
                    pairs[static_cast<std::size_t>(i)] = {toString(key), val};
                }
                for (auto& [k, v] : pairs) obj[k] = v;
                stack_.push_back(VMValue(obj));
                break;
            }
            case OpCode::GetIndex: {
                const VMValue index = pop();
                const VMValue target = pop();
                stack_.push_back(indexInto(target, index));
                break;
            }
            case OpCode::SetIndex: {
                // stack: obj, key, val (val on top)
                const VMValue val   = pop();
                const VMValue key   = pop();
                VMValue       obj   = pop();
                if (std::holds_alternative<VMValue::Object>(obj.data)) {
                    std::get<VMValue::Object>(obj.data)[toString(key)] = val;
                    // Write back to global/local that holds this object
                } else if (std::holds_alternative<VMValue::Array>(obj.data)) {
                    int idx = static_cast<int>(toNumber(key));
                    auto& arr = std::get<VMValue::Array>(obj.data);
                    if (idx >= 0 && idx < static_cast<int>(arr.size())) {
                        arr[static_cast<std::size_t>(idx)] = val;
                    }
                } else {
                    throw std::runtime_error("Cannot set index on non-object/array");
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::SetGlobalIndex: {
                // stack: [key, val]; instruction.a = const index of global name
                const VMValue val = pop();
                const VMValue key = pop();
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                auto found = globals_.find(name);
                if (found == globals_.end()) throw std::runtime_error("Undefined global: " + name);
                VMValue& target = found->second;
                if (std::holds_alternative<VMValue::Object>(target.data)) {
                    std::get<VMValue::Object>(target.data)[toString(key)] = val;
                } else if (std::holds_alternative<VMValue::Array>(target.data)) {
                    int idx = static_cast<int>(toNumber(key));
                    auto& arr = std::get<VMValue::Array>(target.data);
                    if (idx >= 0 && idx < static_cast<int>(arr.size())) {
                        arr[static_cast<std::size_t>(idx)] = val;
                    } else if (idx == static_cast<int>(arr.size())) {
                        arr.push_back(val);
                    }
                } else {
                    throw std::runtime_error("Cannot set index on non-object/array global: " + name);
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::SetLocalIndex: {
                // stack: [key, val]; instruction.a = local slot
                const VMValue val = pop();
                const VMValue key = pop();
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                if (slot >= frame.locals.size()) frame.locals.resize(slot + 1);
                VMValue& target = frame.locals[slot];
                if (std::holds_alternative<VMValue::Object>(target.data)) {
                    std::get<VMValue::Object>(target.data)[toString(key)] = val;
                } else if (std::holds_alternative<VMValue::Array>(target.data)) {
                    int idx = static_cast<int>(toNumber(key));
                    auto& arr = std::get<VMValue::Array>(target.data);
                    if (idx >= 0 && idx < static_cast<int>(arr.size())) {
                        arr[static_cast<std::size_t>(idx)] = val;
                    } else if (idx == static_cast<int>(arr.size())) {
                        arr.push_back(val);
                    }
                } else {
                    throw std::runtime_error("Cannot set index on non-object/array local");
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::AppendGlobal: {
                // stack: [val]; instruction.a = const-idx of global name
                const VMValue val = pop();
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                auto found = globals_.find(name);
                if (found == globals_.end()) throw std::runtime_error("Undefined global for append: " + name);
                VMValue& target = found->second;
                if (std::holds_alternative<VMValue::Array>(target.data)) {
                    std::get<VMValue::Array>(target.data).push_back(val);
                } else {
                    throw std::runtime_error("Cannot append to non-array global: " + name);
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::AppendLocal: {
                // stack: [val]; instruction.a = local slot
                const VMValue val = pop();
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                if (slot >= frame.locals.size()) frame.locals.resize(slot + 1);
                VMValue& target = frame.locals[slot];
                if (std::holds_alternative<VMValue::Array>(target.data)) {
                    std::get<VMValue::Array>(target.data).push_back(val);
                } else {
                    throw std::runtime_error("Cannot append to non-array local");
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::AppendObjGlobal: {
                // stack: [key, val]; instruction.a = const-idx of global object name
                const VMValue val = pop();
                const VMValue key = pop();
                const std::string name = std::get<std::string>(frame.function->constants.at(static_cast<std::size_t>(instruction.a)).data);
                auto found = globals_.find(name);
                if (found == globals_.end()) throw std::runtime_error("Undefined global for obj-append: " + name);
                VMValue& obj = found->second;
                if (std::holds_alternative<VMValue::Object>(obj.data)) {
                    VMValue& inner = std::get<VMValue::Object>(obj.data)[toString(key)];
                    if (std::holds_alternative<VMValue::Array>(inner.data)) {
                        std::get<VMValue::Array>(inner.data).push_back(val);
                    } else {
                        // Initialize as array if not set
                        inner = VMValue::Array{val};
                    }
                } else {
                    throw std::runtime_error("Cannot obj-append to non-object global: " + name);
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::AppendObjLocal: {
                // stack: [key, val]; instruction.a = local slot of the object
                const VMValue val = pop();
                const VMValue key = pop();
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                if (slot >= frame.locals.size()) frame.locals.resize(slot + 1);
                VMValue& obj = frame.locals[slot];
                if (std::holds_alternative<VMValue::Object>(obj.data)) {
                    VMValue& inner = std::get<VMValue::Object>(obj.data)[toString(key)];
                    if (std::holds_alternative<VMValue::Array>(inner.data)) {
                        std::get<VMValue::Array>(inner.data).push_back(val);
                    } else {
                        inner = VMValue::Array{val};
                    }
                } else {
                    throw std::runtime_error("Cannot obj-append to non-object local");
                }
                stack_.push_back(val);
                break;
            }
            case OpCode::Throw: {
                std::string msg = toString(pop());
                throw std::runtime_error(msg);
            }
            case OpCode::Concat: {
                // Pop n values, concatenate as strings (left-to-right)
                int n = instruction.a;
                std::vector<VMValue> parts(static_cast<std::size_t>(n));
                for (int i = n - 1; i >= 0; --i) parts[static_cast<std::size_t>(i)] = pop();
                std::string result;
                for (const auto& p : parts) result += toString(p);
                stack_.push_back(VMValue(result));
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
            case OpCode::GetLocal: {
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                if (slot >= frame.locals.size()) frame.locals.resize(slot + 1);
                stack_.push_back(frame.locals[slot]);
                break;
            }
            case OpCode::SetLocal: {
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                if (slot >= frame.locals.size()) frame.locals.resize(slot + 1);
                frame.locals[slot] = peek();
                break;
            }
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
            case OpCode::Dup:
                stack_.push_back(peek());
                break;
            case OpCode::JumpIfNotNull:
                if (!isNull(peek())) frame.ip = static_cast<std::size_t>(instruction.b);
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
            case OpCode::TryBegin: {
                // a = catch_addr, b = error_name_const_idx
                std::string errorVar = toString(
                    frame.function->constants.at(static_cast<std::size_t>(instruction.b)));
                frame.catchStack.push_back({static_cast<std::size_t>(instruction.a), errorVar});
                break;
            }
            case OpCode::TryEnd: {
                // Pop active catch frame and jump past catch body
                if (!frame.catchStack.empty()) frame.catchStack.pop_back();
                frame.ip = static_cast<std::size_t>(instruction.a);
                break;
            }
            } // end switch
        } catch (const std::exception& e) {
            if (!frame.catchStack.empty()) {
                // Jump to catch block, store error message in global
                CatchFrame cf = frame.catchStack.back();
                frame.catchStack.pop_back();
                if (!cf.error_var.empty()) {
                    globals_[cf.error_var] = VMValue(std::string(e.what()));
                }
                frame.ip = cf.catch_addr;
                // Clear any partial stack values from failed try body
                // (We don't know the exact stack depth, so just continue)
            } else {
                throw; // no active catch frame, re-propagate
            }
        }
        } // end while

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
        if (name == "sqrt") return VMValue(args.empty() ? 0.0 : std::sqrt(toNumber(args[0])));
        if (name == "abs") return VMValue(args.empty() ? 0.0 : std::abs(toNumber(args[0])));
        if (name == "pow") return VMValue(args.size() < 2 ? 0.0 : std::pow(toNumber(args[0]), toNumber(args[1])));
        if (name == "sin") return VMValue(args.empty() ? 0.0 : std::sin(toNumber(args[0])));
        if (name == "cos") return VMValue(args.empty() ? 0.0 : std::cos(toNumber(args[0])));
        if (name == "tan") return VMValue(args.empty() ? 0.0 : std::tan(toNumber(args[0])));
        if (name == "log") return VMValue(args.empty() ? 0.0 : std::log(toNumber(args[0])));
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
        if (name == "__optional_get") {
            // args[0] = object, args[1] = key
            if (args.size() < 2 || isNull(args[0])) return VMValue();
            const std::string key = toString(args[1]);
            if (std::holds_alternative<VMValue::Object>(args[0].data)) {
                const auto& obj = std::get<VMValue::Object>(args[0].data);
                auto it = obj.find(key);
                if (it != obj.end()) return it->second;
                return VMValue();
            }
            if (std::holds_alternative<VMValue::Array>(args[0].data)) {
                const auto& arr = std::get<VMValue::Array>(args[0].data);
                try {
                    const int i = static_cast<int>(std::stod(key));
                    if (i >= 0 && i < static_cast<int>(arr.size())) return arr[i];
                } catch (...) {}
                return VMValue();
            }
            return VMValue();
        }
        if (name == "crypto.sha256" || name == "hash.sha256") {
            if (args.empty()) return VMValue(std::string(""));
            std::string input = toString(args[0]);
            auto rotr32 = [](uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); };
            static const uint32_t K256[64]={
                0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
                0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
                0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
                0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
                0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
                0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
                0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
                0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
            };
            std::vector<uint8_t> msg(input.begin(), input.end());
            uint64_t origBits = msg.size() * 8;
            msg.push_back(0x80);
            while ((msg.size() % 64) != 56) msg.push_back(0);
            for (int i = 7; i >= 0; --i) msg.push_back((origBits >> (i*8)) & 0xFF);
            uint32_t h0=0x6a09e667,h1=0xbb67ae85,h2=0x3c6ef372,h3=0xa54ff53a,h4=0x510e527f,h5=0x9b05688c,h6=0x1f83d9ab,h7=0x5be0cd19;
            for (size_t c=0; c<msg.size(); c+=64) {
                uint32_t w[64]={};
                for (int i=0;i<16;++i) w[i]=(msg[c+i*4]<<24)|(msg[c+i*4+1]<<16)|(msg[c+i*4+2]<<8)|msg[c+i*4+3];
                for (int i=16;i<64;++i){uint32_t s0=rotr32(w[i-15],7)^rotr32(w[i-15],18)^(w[i-15]>>3);uint32_t s1=rotr32(w[i-2],17)^rotr32(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
                uint32_t a=h0,b=h1,cc=h2,d=h3,e=h4,f=h5,g=h6,hh=h7;
                for(int i=0;i<64;++i){uint32_t S1=rotr32(e,6)^rotr32(e,11)^rotr32(e,25);uint32_t ch=(e&f)^(~e&g);uint32_t temp1=hh+S1+ch+K256[i]+w[i];uint32_t S0=rotr32(a,2)^rotr32(a,13)^rotr32(a,22);uint32_t maj=(a&b)^(a&cc)^(b&cc);uint32_t temp2=S0+maj;hh=g;g=f;f=e;e=d+temp1;d=cc;cc=b;b=a;a=temp1+temp2;}
                h0+=a;h1+=b;h2+=cc;h3+=d;h4+=e;h5+=f;h6+=g;h7+=hh;
            }
            std::ostringstream oss; oss<<std::hex<<std::setfill('0');
            for (auto v:{h0,h1,h2,h3,h4,h5,h6,h7}) oss<<std::setw(8)<<v;
            return VMValue(oss.str());
        }
        if (name == "crypto.md5") {
            if (args.empty()) return VMValue(std::string(""));
            std::string s = toString(args[0]);
            std::vector<uint8_t> msg(s.begin(), s.end());
            uint64_t origLen = msg.size() * 8;
            msg.push_back(0x80);
            while ((msg.size() % 64) != 56) msg.push_back(0);
            for (int i=0;i<8;++i) msg.push_back((origLen>>(i*8))&0xFF);
            uint32_t a0=0x67452301,b0=0xefcdab89,c0=0x98badcfe,d0=0x10325476;
            static const uint32_t S[]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
            static const uint32_t T[]={0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
            auto lrot=[](uint32_t x,uint32_t n){return (x<<n)|(x>>(32-n));};
            for (size_t c=0;c<msg.size();c+=64){
                uint32_t M[16]; for(int i=0;i<16;++i) M[i]=msg[c+i*4]|(msg[c+i*4+1]<<8)|(msg[c+i*4+2]<<16)|(msg[c+i*4+3]<<24);
                uint32_t A=a0,B=b0,C=c0,D=d0;
                for(int i=0;i<64;++i){uint32_t F2,g;if(i<16){F2=(B&C)|(~B&D);g=i;}else if(i<32){F2=(D&B)|(~D&C);g=(5*i+1)%16;}else if(i<48){F2=B^C^D;g=(3*i+5)%16;}else{F2=C^(B|~D);g=(7*i)%16;}F2=F2+A+T[i]+M[g];A=D;D=C;C=B;B=B+lrot(F2,S[i]);}
                a0+=A;b0+=B;c0+=C;d0+=D;
            }
            std::ostringstream oss; oss<<std::hex<<std::setfill('0');
            auto le=[&](uint32_t v){for(int i=0;i<4;++i) oss<<std::setw(2)<<((v>>(i*8))&0xFF);};
            le(a0);le(b0);le(c0);le(d0);
            return VMValue(oss.str());
        }
        if (name == "crypto.base64_encode") {
            if (args.empty()) return VMValue(std::string(""));
            const std::string input = toString(args[0]);
            static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out; int val=0, bits=-6;
            for (unsigned char c : input) { val=(val<<8)+c; bits+=8; while(bits>=0){out.push_back(tbl[(val>>bits)&0x3F]);bits-=6;} }
            if (bits>-6) out.push_back(tbl[((val<<8)>>(bits+8))&0x3F]);
            while (out.size()%4) out.push_back('=');
            return VMValue(out);
        }
        if (name == "crypto.base64_decode") {
            if (args.empty()) return VMValue(std::string(""));
            const std::string input = toString(args[0]);
            static const int lut[256]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
            std::string out; int val=0,bits=-8;
            for (unsigned char c : input) { int d=lut[c]; if(d==-1) break; if(d==-2) continue; val=(val<<6)+d; bits+=6; if(bits>=0){out.push_back((char)((val>>bits)&0xFF));bits-=8;} }
            return VMValue(out);
        }
        if (name == "crypto.uuid") {
            std::random_device rd; std::mt19937_64 rng(rd()); std::uniform_int_distribution<uint64_t> dist;
            uint64_t hi=dist(rng),lo=dist(rng);
            hi=(hi&0xFFFFFFFFFFFF0FFFull)|0x0000000000004000ull;
            lo=(lo&0x3FFFFFFFFFFFFFFFull)|0x8000000000000000ull;
            std::ostringstream oss; oss<<std::hex<<std::setfill('0');
            oss<<std::setw(8)<<((hi>>32)&0xFFFFFFFF)<<"-"<<std::setw(4)<<((hi>>16)&0xFFFF)<<"-"<<std::setw(4)<<(hi&0xFFFF)<<"-"<<std::setw(4)<<((lo>>48)&0xFFFF)<<"-"<<std::setw(12)<<(lo&0xFFFFFFFFFFFFull);
            return VMValue(oss.str());
        }
        if (name == "crypto.random_bytes") {
            int n = args.empty() ? 16 : static_cast<int>(toNumber(args[0]));
            if (n<1) n=1; if (n>1024) n=1024;
            std::random_device rd; std::mt19937 rng(rd()); std::uniform_int_distribution<int> dist(0,255);
            std::ostringstream oss; oss<<std::hex<<std::setfill('0');
            for (int i=0;i<n;++i) oss<<std::setw(2)<<dist(rng);
            return VMValue(oss.str());
        }
        if (name == "crypto.hmac_sha256") {
            if (args.size() < 2) return VMValue(std::string(""));
            std::string msg2=toString(args[0]), key2=toString(args[1]);
            if (key2.size()>64) { auto kv={VMValue(key2)}; key2=toString(*callDefaultBuiltin("crypto.sha256",{VMValue(key2)})); }
            while (key2.size()<64) key2.push_back('\0');
            std::string opad(64,'\x5c'), ipad(64,'\x36');
            for (size_t i=0;i<64;++i){opad[i]^=key2[i];ipad[i]^=key2[i];}
            std::string inner=toString(*callDefaultBuiltin("crypto.sha256",{VMValue(ipad+msg2)}));
            std::string inner_bytes;
            for (size_t i=0;i<inner.size();i+=2) inner_bytes.push_back((char)std::stoi(inner.substr(i,2),nullptr,16));
            return VMValue(toString(*callDefaultBuiltin("crypto.sha256",{VMValue(opad+inner_bytes)})));
        }
        if (name == "datetime.now") {
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &t);
#else
            localtime_r(&t, &tm_buf);
#endif
            VMValue::Object obj;
            obj["year"]      = VMValue(static_cast<double>(tm_buf.tm_year + 1900));
            obj["month"]     = VMValue(static_cast<double>(tm_buf.tm_mon + 1));
            obj["day"]       = VMValue(static_cast<double>(tm_buf.tm_mday));
            obj["hour"]      = VMValue(static_cast<double>(tm_buf.tm_hour));
            obj["minute"]    = VMValue(static_cast<double>(tm_buf.tm_min));
            obj["second"]    = VMValue(static_cast<double>(tm_buf.tm_sec));
            obj["ms"]        = VMValue(static_cast<double>(ms.count()));
            obj["timestamp"] = VMValue(static_cast<double>(t));
            return VMValue(obj);
        }
        if (name == "datetime.timestamp") {
            return VMValue(static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        }
        if (name == "datetime.format") {
            if (args.size() < 2) return VMValue(std::string(""));
            time_t t = static_cast<time_t>(toNumber(args[0]));
            std::string fmt = toString(args[1]);
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &t);
#else
            localtime_r(&t, &tm_buf);
#endif
            char buf[256];
            std::strftime(buf, sizeof(buf), fmt.c_str(), &tm_buf);
            return VMValue(std::string(buf));
        }
        if (name == "path.join") {
            if (args.empty()) return VMValue(std::string(""));
            std::filesystem::path p(toString(args[0]));
            for (size_t i = 1; i < args.size(); ++i) p /= toString(args[i]);
            return VMValue(p.string());
        }
        if (name == "path.basename") {
            if (args.empty()) return VMValue(std::string(""));
            return VMValue(std::filesystem::path(toString(args[0])).filename().string());
        }
        if (name == "path.dirname") {
            if (args.empty()) return VMValue(std::string(""));
            return VMValue(std::filesystem::path(toString(args[0])).parent_path().string());
        }
        if (name == "path.ext") {
            if (args.empty()) return VMValue(std::string(""));
            return VMValue(std::filesystem::path(toString(args[0])).extension().string());
        }
        if (name == "path.exists") {
            if (args.empty()) return VMValue(false);
            return VMValue(std::filesystem::exists(toString(args[0])));
        }
        if (name == "path.abs") {
            if (args.empty()) return VMValue(std::string(""));
            std::error_code ec;
            auto p = std::filesystem::absolute(toString(args[0]), ec);
            return VMValue(ec ? toString(args[0]) : p.string());
        }
        if (name == "path.stem") {
            if (args.empty()) return VMValue(std::string(""));
            return VMValue(std::filesystem::path(toString(args[0])).stem().string());
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

        // ── Math extras ───────────────────────────────────────────────────────
        if (name == "atan2") return VMValue(std::atan2(toNumber(args[0]), toNumber(args[1])));
        if (name == "hypot") return VMValue(std::hypot(toNumber(args[0]), toNumber(args[1])));
        if (name == "atan")  return VMValue(std::atan(toNumber(args[0])));
        if (name == "asin")  return VMValue(std::asin(toNumber(args[0])));
        if (name == "acos")  return VMValue(std::acos(toNumber(args[0])));
        if (name == "exp")   return VMValue(std::exp(toNumber(args[0])));
        if (name == "log2")  return VMValue(std::log2(toNumber(args[0])));
        if (name == "log10") return VMValue(std::log10(toNumber(args[0])));

        // ── String extras ─────────────────────────────────────────────────────
        if (name == "startswith") {
            if (args.size() >= 2) {
                auto s = toString(args[0]); auto p = toString(args[1]);
                return VMValue(s.size() >= p.size() && s.substr(0, p.size()) == p);
            }
            return VMValue(false);
        }
        if (name == "endswith") {
            if (args.size() >= 2) {
                auto s = toString(args[0]); auto p = toString(args[1]);
                return VMValue(s.size() >= p.size() && s.substr(s.size() - p.size()) == p);
            }
            return VMValue(false);
        }
        if (name == "indexof") {
            if (args.size() >= 2) {
                auto s = toString(args[0]); auto n = toString(args[1]);
                auto pos = s.find(n);
                return VMValue(pos == std::string::npos ? -1.0 : static_cast<double>(pos));
            }
            return VMValue(-1.0);
        }
        if (name == "repeat") {
            if (args.size() >= 2) {
                auto s = toString(args[0]); int n = static_cast<int>(toNumber(args[1]));
                std::string r; for (int i = 0; i < n; ++i) r += s; return VMValue(r);
            }
            return VMValue(std::string(""));
        }
        if (name == "char_code") {
            if (!args.empty()) {
                auto s = toString(args[0]);
                return VMValue(s.empty() ? 0.0 : static_cast<double>(static_cast<unsigned char>(s[0])));
            }
            return VMValue(0.0);
        }
        if (name == "char_from" || name == "char") {
            if (!args.empty())
                return VMValue(std::string(1, static_cast<char>(static_cast<int>(toNumber(args[0])))));
            return VMValue(std::string(""));
        }

        return std::nullopt;
    }
};
