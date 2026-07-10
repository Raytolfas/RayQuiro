#pragma once

#include <cstdint>
#include <ostream>

#ifdef _WIN32
#ifdef SOCKET
#undef SOCKET
#endif
#ifdef INVALID_SOCKET
#undef INVALID_SOCKET
#endif
#ifdef SOCKET_ERROR
#undef SOCKET_ERROR
#endif
#ifdef AF_INET
#undef AF_INET
#endif
#ifdef SOCK_STREAM
#undef SOCK_STREAM
#endif
#ifdef IPPROTO_TCP
#undef IPPROTO_TCP
#endif
#ifdef SOL_SOCKET
#undef SOL_SOCKET
#endif
#ifdef SO_REUSEADDR
#undef SO_REUSEADDR
#endif
#ifdef SOMAXCONN
#undef SOMAXCONN
#endif
#ifdef INADDR_ANY
#undef INADDR_ANY
#endif

using SOCKET = std::uintptr_t;
static constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(~static_cast<SOCKET>(0));
static constexpr int SOCKET_ERROR = -1;
static constexpr int AF_INET = 2;
static constexpr int SOCK_STREAM = 1;
static constexpr int IPPROTO_TCP = 6;
static constexpr int SOL_SOCKET = 0xffff;
static constexpr int SO_REUSEADDR = 0x0004;
static constexpr int SOMAXCONN = 0x7fffffff;
static constexpr unsigned long INADDR_ANY = 0u;
using u_short = unsigned short;

struct in_addr {
    unsigned long s_addr;
};

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    in_addr sin_addr;
    char sin_zero[8];
};

struct WSADATA {
    unsigned short wVersion;
    unsigned short wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
};

extern "C" {
    unsigned short htons(unsigned short value);
    unsigned long htonl(unsigned long value);
    int inet_pton(int af, const char* src, void* dst);
    SOCKET socket(int af, int type, int protocol);
    int bind(SOCKET s, const sockaddr* name, int namelen);
    int listen(SOCKET s, int backlog);
    SOCKET accept(SOCKET s, sockaddr* addr, int* addrlen);
    int recv(SOCKET s, char* buf, int len, int flags);
    int send(SOCKET s, const char* buf, int len, int flags);
    int setsockopt(SOCKET s, int level, int optname, const void* optval, int optlen);
    int closesocket(SOCKET s);
    int WSAStartup(unsigned short wVersionRequested, WSADATA* lpWSAData);
    int WSACleanup();
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef RAYQUIRO_SOCKET_DEFINED
#define RAYQUIRO_SOCKET_DEFINED
using SOCKET = int;
static constexpr SOCKET INVALID_SOCKET = -1;
static constexpr int SOCKET_ERROR = -1;
inline int closesocket(SOCKET socket) { return ::close(socket); }
#endif
#endif

struct RuntimeFeatures {
    bool usesApp = false;
    bool usesUi = false;
    bool usesWeb = false;
    bool usesEngine = false;
    bool usesFs = false;
    bool usesEnv = false;
    bool usesProcess = false;
    bool usesNativeModules = false;
    bool usesOs = false;
    bool usesNet = false;
    bool usesHttp = false;
    bool usesDb = false;
};

class RuntimeEmitter {
public:
    static void emit(std::ostream& out, const RuntimeFeatures& features) {
        emitCoreRuntime(out);
        if (features.usesFs) {
            emitFsRuntime(out);
        }
        if (features.usesProcess) {
            emitProcessRuntime(out);
        }
        if (features.usesEnv) {
            emitEnvRuntime(out);
        }
        if (features.usesOs) {
            emitOsRuntime(out);
        }
        if (features.usesNet) {
            emitNetRuntime(out);
        }
        if (features.usesHttp) {
            emitHttpRuntime(out);
        }
        if (features.usesDb) {
            emitDbRuntime(out);
        }
        if (features.usesUi) {
            emitUiRuntime(out);
        }
        if (features.usesWeb) {
            emitWebRuntime(out);
        }
        if (features.usesApp) {
            emitAppRuntime(out);
        }
        if (features.usesEngine) {
            emitEngineRuntime(out);
        }
    }

private:
    static void emitCoreRuntime(std::ostream& out) {
        out << R"(namespace rq {
struct Value {
    using Arr = std::shared_ptr<std::vector<Value>>;
    using Obj = std::shared_ptr<std::unordered_map<std::string, Value>>;

    std::variant<std::monostate, double, std::string, bool, Arr, Obj> data;

    Value() : data(std::monostate{}) {}
    Value(double v) : data(v) {}
    Value(const std::string& s) : data(s) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(bool b) : data(b) {}

    static Value array() { return Value(Arr(new std::vector<Value>())); }
    static Value object() { return Value(Obj(new std::unordered_map<std::string, Value>())); }

private:
    Value(Arr arrayValue) : data(arrayValue) {}
    Value(Obj objectValue) : data(objectValue) {}

public:
    Arr as_array() const { return std::get<Arr>(data); }
    Obj as_object() const { return std::get<Obj>(data); }
};

inline bool is_null(const Value& value) { return std::holds_alternative<std::monostate>(value.data); }
inline bool is_number(const Value& value) { return std::holds_alternative<double>(value.data); }
inline bool is_string(const Value& value) { return std::holds_alternative<std::string>(value.data); }
inline bool is_bool(const Value& value) { return std::holds_alternative<bool>(value.data); }
inline bool is_array(const Value& value) { return std::holds_alternative<Value::Arr>(value.data); }
inline bool is_object(const Value& value) { return std::holds_alternative<Value::Obj>(value.data); }

inline double to_number(const Value& value) {
    if (is_number(value)) return std::get<double>(value.data);
    if (is_bool(value)) return std::get<bool>(value.data) ? 1.0 : 0.0;
    if (is_string(value)) {
        try {
            return std::stod(std::get<std::string>(value.data));
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

inline std::string number_to_string(double value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string to_string(const Value& value) {
    if (is_null(value)) return "null";
    if (is_number(value)) return number_to_string(std::get<double>(value.data));
    if (is_bool(value)) return std::get<bool>(value.data) ? "true" : "false";
    if (is_string(value)) return std::get<std::string>(value.data);
    if (is_array(value)) {
        auto arrayValue = std::get<Value::Arr>(value.data);
        std::string result = "[";
        for (size_t i = 0; i < arrayValue->size(); ++i) {
            if (i) result += ", ";
            result += to_string((*arrayValue)[i]);
        }
        result += "]";
        return result;
    }
    if (is_object(value)) {
        auto objectValue = std::get<Value::Obj>(value.data);
        std::string result = "{";
        size_t index = 0;
        for (const auto& pair : *objectValue) {
            if (index++) result += ", ";
            result += pair.first + ": " + to_string(pair.second);
        }
        result += "}";
        return result;
    }
    return "";
}

inline bool truthy(const Value& value) {
    if (is_null(value)) return false;
    if (is_bool(value)) return std::get<bool>(value.data);
    if (is_number(value)) return std::get<double>(value.data) != 0.0;
    if (is_string(value)) return !std::get<std::string>(value.data).empty();
    if (is_array(value)) return !std::get<Value::Arr>(value.data)->empty();
    if (is_object(value)) return !std::get<Value::Obj>(value.data)->empty();
    return false;
}

inline Value array(const std::vector<Value>& items) {
    Value value = Value::array();
    *value.as_array() = items;
    return value;
}

inline Value object(const std::unordered_map<std::string, Value>& items = {}) {
    Value value = Value::object();
    *value.as_object() = items;
    return value;
}

inline Value add(const Value& left, const Value& right) {
    if (is_number(left) && is_number(right)) {
        return Value(std::get<double>(left.data) + std::get<double>(right.data));
    }
    return Value(to_string(left) + to_string(right));
}

inline Value sub(const Value& left, const Value& right) { return Value(to_number(left) - to_number(right)); }
inline Value mul(const Value& left, const Value& right) { return Value(to_number(left) * to_number(right)); }
inline Value div(const Value& left, const Value& right) { return Value(to_number(left) / to_number(right)); }
inline Value mod(const Value& left, const Value& right) { return Value(std::fmod(to_number(left), to_number(right))); }

inline Value eq(const Value& left, const Value& right) {
    if (is_null(left) && is_null(right)) return Value(true);
    if (is_number(left) && is_number(right)) return Value(std::get<double>(left.data) == std::get<double>(right.data));
    if (is_bool(left) && is_bool(right)) return Value(std::get<bool>(left.data) == std::get<bool>(right.data));
    if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) == std::get<std::string>(right.data));
    return Value(false);
}

inline Value neq(const Value& left, const Value& right) { return Value(!truthy(eq(left, right))); }
inline Value lt(const Value& left, const Value& right) {
    if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) < std::get<std::string>(right.data));
    return Value(to_number(left) < to_number(right));
}
inline Value lte(const Value& left, const Value& right) {
    if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) <= std::get<std::string>(right.data));
    return Value(to_number(left) <= to_number(right));
}
inline Value gt(const Value& left, const Value& right) {
    if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) > std::get<std::string>(right.data));
    return Value(to_number(left) > to_number(right));
}
inline Value gte(const Value& left, const Value& right) {
    if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) >= std::get<std::string>(right.data));
    return Value(to_number(left) >= to_number(right));
}

inline Value index(const Value& target, const Value& indexValue) {
    if (is_array(target)) {
        auto items = target.as_array();
        const size_t indexNumber = static_cast<size_t>(to_number(indexValue));
        if (indexNumber < items->size()) {
            return (*items)[indexNumber];
        }
        return Value();
    }

    if (is_string(target)) {
        const std::string& text = std::get<std::string>(target.data);
        const size_t indexNumber = static_cast<size_t>(to_number(indexValue));
        if (indexNumber < text.size()) {
            return Value(std::string(1, text[indexNumber]));
        }
        return Value();
    }

    if (is_object(target)) {
        auto objectValue = target.as_object();
        const std::string key = to_string(indexValue);
        const auto found = objectValue->find(key);
        if (found != objectValue->end()) {
            return found->second;
        }
        return Value();
    }

    return Value();
}

inline Value print(const std::vector<Value>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) std::cout << " ";
        std::cout << to_string(args[i]);
    }
    std::cout << std::endl;
    return Value();
}

inline Value len(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    const Value& value = args[0];
    if (is_string(value)) return Value(static_cast<double>(std::get<std::string>(value.data).size()));
    if (is_array(value)) return Value(static_cast<double>(value.as_array()->size()));
    if (is_object(value)) return Value(static_cast<double>(value.as_object()->size()));
    return Value(0.0);
}

inline Value str(const std::vector<Value>& args) {
    if (args.empty()) return Value("");
    return Value(to_string(args[0]));
}

inline Value num(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    return Value(to_number(args[0]));
}

inline Value to_bool(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    return Value(truthy(args[0]));
}

inline Value type_name(const std::vector<Value>& args) {
    if (args.empty()) return Value("null");
    const Value& value = args[0];
    if (is_null(value)) return Value("null");
    if (is_number(value)) return Value("number");
    if (is_string(value)) return Value("string");
    if (is_bool(value)) return Value("bool");
    if (is_array(value)) return Value("array");
    if (is_object(value)) return Value("object");
    return Value("unknown");
}

inline Value range(const std::vector<Value>& args) {
    double start = 0.0;
    double end = 0.0;
    double step = 1.0;

    if (args.size() == 1) {
        end = to_number(args[0]);
    } else if (args.size() >= 2) {
        start = to_number(args[0]);
        end = to_number(args[1]);
    }
    if (args.size() >= 3) {
        step = to_number(args[2]);
    }

    if (step == 0.0) {
        return array({});
    }

    std::vector<Value> result;
    if (step > 0.0) {
        for (double value = start; value < end; value += step) {
            result.push_back(Value(value));
        }
    } else {
        for (double value = start; value > end; value += step) {
            result.push_back(Value(value));
        }
    }

    return array(result);
}

inline Value push(const std::vector<Value>& args) {
    if (args.size() < 2 || !is_array(args[0])) return Value();
    Value arrayValue = args[0];
    arrayValue.as_array()->push_back(args[1]);
    return arrayValue;
}

inline Value pop(const std::vector<Value>& args) {
    if (args.empty() || !is_array(args[0])) return Value();
    Value arrayValue = args[0];
    auto items = arrayValue.as_array();
    if (items->empty()) return Value();
    Value result = items->back();
    items->pop_back();
    return result;
}

inline Value join(const std::vector<Value>& args) {
    if (args.empty() || !is_array(args[0])) return Value("");
    const std::string separator = args.size() > 1 ? to_string(args[1]) : "";
    auto items = args[0].as_array();
    std::string result;
    for (size_t i = 0; i < items->size(); ++i) {
        if (i) result += separator;
        result += to_string((*items)[i]);
    }
    return Value(result);
}

inline Value split(const std::vector<Value>& args) {
    if (args.empty()) return array({});
    const std::string text = to_string(args[0]);
    const std::string separator = args.size() > 1 ? to_string(args[1]) : "";
    std::vector<Value> result;

    if (separator.empty()) {
        for (char c : text) {
            result.push_back(Value(std::string(1, c)));
        }
        return array(result);
    }

    size_t start = 0;
    while (true) {
        const size_t found = text.find(separator, start);
        if (found == std::string::npos) {
            result.push_back(Value(text.substr(start)));
            break;
        }
        result.push_back(Value(text.substr(start, found - start)));
        start = found + separator.size();
    }
    return array(result);
}

inline Value upper(const std::vector<Value>& args) {
    if (args.empty()) return Value("");
    std::string result = to_string(args[0]);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return Value(result);
}

inline Value lower(const std::vector<Value>& args) {
    if (args.empty()) return Value("");
    std::string result = to_string(args[0]);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return Value(result);
}

inline Value contains(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);
    const Value& container = args[0];
    const Value& needle = args[1];

    if (is_string(container)) {
        return Value(std::get<std::string>(container.data).find(to_string(needle)) != std::string::npos);
    }

    if (is_array(container)) {
        for (const auto& item : *container.as_array()) {
            if (truthy(eq(item, needle))) {
                return Value(true);
            }
        }
    }

    return Value(false);
}

inline std::string trim_copy(const std::string& value) {
    const auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

inline Value trim(const std::vector<Value>& args) {
    if (args.empty()) return Value("");
    return Value(trim_copy(to_string(args[0])));
}

inline Value replace(const std::vector<Value>& args) {
    if (args.size() < 3) return Value(args.empty() ? "" : to_string(args[0]));
    std::string text = to_string(args[0]);
    const std::string needle = to_string(args[1]);
    const std::string replacement = to_string(args[2]);
    if (needle.empty()) return Value(text);

    size_t start = 0;
    while ((start = text.find(needle, start)) != std::string::npos) {
        text.replace(start, needle.size(), replacement);
        start += replacement.size();
    }
    return Value(text);
}

inline int size_of(const Value& value) {
    if (is_string(value)) return static_cast<int>(std::get<std::string>(value.data).size());
    if (is_array(value)) return static_cast<int>(value.as_array()->size());
    if (is_object(value)) return static_cast<int>(value.as_object()->size());
    return 0;
}

inline Value slice(const std::vector<Value>& args) {
    if (args.empty()) return Value();
    const Value& source = args[0];
    const int start = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 0;
    const int end = args.size() > 2 ? static_cast<int>(to_number(args[2])) : size_of(source);

    if (is_string(source)) {
        const std::string text = std::get<std::string>(source.data);
        const int safeStart = std::max(0, start);
        const int safeEnd = std::max(safeStart, std::min(static_cast<int>(text.size()), end));
        return Value(text.substr(static_cast<size_t>(safeStart), static_cast<size_t>(safeEnd - safeStart)));
    }

    if (is_array(source)) {
        auto items = source.as_array();
        const int safeStart = std::max(0, start);
        const int safeEnd = std::max(safeStart, std::min(static_cast<int>(items->size()), end));
        std::vector<Value> result;
        for (int i = safeStart; i < safeEnd; ++i) {
            result.push_back((*items)[static_cast<size_t>(i)]);
        }
        return array(result);
    }

    return Value();
}

inline Value floor_value(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    return Value(std::floor(to_number(args[0])));
}

inline Value ceil_value(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    return Value(std::ceil(to_number(args[0])));
}

inline Value round_value(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    return Value(std::round(to_number(args[0])));
}

inline Value min_value(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    double result = to_number(args[0]);
    for (size_t i = 1; i < args.size(); ++i) {
        result = std::min(result, to_number(args[i]));
    }
    return Value(result);
}

inline Value max_value(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    double result = to_number(args[0]);
    for (size_t i = 1; i < args.size(); ++i) {
        result = std::max(result, to_number(args[i]));
    }
    return Value(result);
}

inline Value clamp_value(const std::vector<Value>& args) {
    if (args.size() < 3) return Value(args.empty() ? 0.0 : to_number(args[0]));
    return Value(std::clamp(to_number(args[0]), to_number(args[1]), to_number(args[2])));
}

inline Value sleep_ms(const std::vector<Value>& args) {
    const int duration = args.empty() ? 0 : static_cast<int>(to_number(args[0]));
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    return Value();
}

inline Value clock_ms(const std::vector<Value>& args) {
    (void)args;
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
}

inline Value unix_ms(const std::vector<Value>& args) {
    (void)args;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
}

inline std::string json_escape_text(const std::string& value) {
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

inline std::string json_stringify_value(const Value& value) {
    if (is_null(value)) return "null";
    if (is_number(value)) return number_to_string(std::get<double>(value.data));
    if (is_bool(value)) return std::get<bool>(value.data) ? "true" : "false";
    if (is_string(value)) return "\"" + json_escape_text(std::get<std::string>(value.data)) + "\"";
    if (is_array(value)) {
        std::string result = "[";
        const auto items = value.as_array();
        for (size_t i = 0; i < items->size(); ++i) {
            if (i) result += ",";
            result += json_stringify_value((*items)[i]);
        }
        result += "]";
        return result;
    }
    if (is_object(value)) {
        std::string result = "{";
        bool first = true;
        for (const auto& pair : *value.as_object()) {
            if (!first) result += ",";
            first = false;
            result += "\"" + json_escape_text(pair.first) + "\":" + json_stringify_value(pair.second);
        }
        result += "}";
        return result;
    }
    return "null";
}

inline void json_skip_ws(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

inline std::string json_parse_string(const std::string& text, size_t& pos) {
    if (pos >= text.size() || text[pos] != '"') {
        throw std::runtime_error("Invalid JSON string.");
    }
    ++pos;
    std::string result;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') {
            return result;
        }
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

inline Value json_parse_value(const std::string& text, size_t& pos) {
    json_skip_ws(text, pos);
    if (pos >= text.size()) {
        throw std::runtime_error("Unexpected end of JSON input.");
    }

    if (text[pos] == '"') {
        return Value(json_parse_string(text, pos));
    }
    if (text[pos] == '[') {
        ++pos;
        std::vector<Value> values;
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            return array(values);
        }
        while (pos < text.size()) {
            values.push_back(json_parse_value(text, pos));
            json_skip_ws(text, pos);
            if (pos < text.size() && text[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < text.size() && text[pos] == ']') {
                ++pos;
                return array(values);
            }
            throw std::runtime_error("Invalid JSON array.");
        }
        throw std::runtime_error("Unterminated JSON array.");
    }
    if (text[pos] == '{') {
        ++pos;
        Value objectValue = object();
        auto out = objectValue.as_object();
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            return objectValue;
        }
        while (pos < text.size()) {
            json_skip_ws(text, pos);
            const std::string key = json_parse_string(text, pos);
            json_skip_ws(text, pos);
            if (pos >= text.size() || text[pos] != ':') {
                throw std::runtime_error("Invalid JSON object.");
            }
            ++pos;
            (*out)[key] = json_parse_value(text, pos);
            json_skip_ws(text, pos);
            if (pos < text.size() && text[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < text.size() && text[pos] == '}') {
                ++pos;
                return objectValue;
            }
            throw std::runtime_error("Invalid JSON object.");
        }
        throw std::runtime_error("Unterminated JSON object.");
    }
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        return Value(true);
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        return Value(false);
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        return Value();
    }

    size_t end = pos;
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
    return Value(number);
}

inline Value json_parse(const std::vector<Value>& args) {
    if (args.empty()) return Value();
    const std::string text = to_string(args[0]);
    size_t pos = 0;
    Value result = json_parse_value(text, pos);
    json_skip_ws(text, pos);
    if (pos != text.size()) {
        throw std::runtime_error("Unexpected trailing characters in JSON.");
    }
    return result;
}

inline Value json_stringify(const std::vector<Value>& args) {
    if (args.empty()) return Value("null");
    return Value(json_stringify_value(args[0]));
}

inline std::string native_module_extension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

inline bool is_known_builtin_namespace(const std::string& name) {
    return name == "app" ||
        name == "ui" ||
        name == "web" ||
        name == "engine" ||
        name == "fs" ||
        name == "env" ||
        name == "process" ||
        name == "time" ||
        name == "json";
}

inline bool is_overridable_builtin_namespace(const std::string& name) {
    return name == "app" || name == "ui" || name == "web" || name == "engine";
}

inline bool is_native_fallback_error(const std::string& text) {
    return text.find("does not export") != std::string::npos ||
           text.find("__RQIO_FALLBACK__") != std::string::npos;
}

struct NativeModule {
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
    using InvokeFn = int (*)(const char*, const char*, char**, char**);
    using FreeFn = void (*)(char*);
    InvokeFn invoke = nullptr;
    FreeFn freeMemory = nullptr;
};

inline std::unordered_map<std::string, NativeModule>& native_modules() {
    static std::unordered_map<std::string, NativeModule> modules;
    return modules;
}

inline std::vector<std::filesystem::path> native_module_candidates(const std::string& moduleName) {
    const std::string fileName = moduleName + native_module_extension();
    const std::filesystem::path cwd = std::filesystem::current_path();
    std::vector<std::filesystem::path> candidates = {
        cwd / ".rq_modules" / "native" / fileName,
        cwd / "modules" / fileName
    };

    if (const char* exeDir = std::getenv("RAYQUIRO_EXE_DIR")) {
        const std::filesystem::path exeRoot = exeDir;
        candidates.push_back(exeRoot / "modules" / fileName);
    }

    if (const char* userProfile = std::getenv("USERPROFILE")) {
        candidates.push_back(std::filesystem::path(userProfile) / ".rqio" / "modules" / fileName);
    } else if (const char* home = std::getenv("HOME")) {
        candidates.push_back(std::filesystem::path(home) / ".rqio" / "modules" / fileName);
    }

#ifdef _WIN32
    if (const char* programFiles = std::getenv("ProgramFiles")) {
        candidates.push_back(std::filesystem::path(programFiles) / "RayQuiro" / "modules" / fileName);
    }
#else
    candidates.push_back(std::filesystem::path("/usr/local/lib/rayquiro/modules") / fileName);
#endif
    return candidates;
}

inline bool has_native_module(const std::string& moduleName) {
    for (const auto& candidate : native_module_candidates(moduleName)) {
        if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
            return true;
        }
    }
    return false;
}

inline NativeModule& load_native_module(const std::string& moduleName) {
    auto& modules = native_modules();
    const auto found = modules.find(moduleName);
    if (found != modules.end()) {
        return found->second;
    }

    NativeModule module;
    std::filesystem::path loadedFrom;
    for (const auto& candidate : native_module_candidates(moduleName)) {
        if (!std::filesystem::exists(candidate) || !std::filesystem::is_regular_file(candidate)) {
            continue;
        }
#ifdef _WIN32
        module.handle = LoadLibraryA(candidate.string().c_str());
#else
        module.handle = dlopen(candidate.string().c_str(), RTLD_NOW);
#endif
        if (module.handle != nullptr) {
            loadedFrom = candidate;
            break;
        }
    }

    if (module.handle == nullptr) {
        throw std::runtime_error("Native module was not found: rayquiro." + moduleName);
    }

#ifdef _WIN32
    module.invoke = reinterpret_cast<NativeModule::InvokeFn>(GetProcAddress(module.handle, "rqm_invoke"));
    module.freeMemory = reinterpret_cast<NativeModule::FreeFn>(GetProcAddress(module.handle, "rqm_free"));
#else
    module.invoke = reinterpret_cast<NativeModule::InvokeFn>(dlsym(module.handle, "rqm_invoke"));
    module.freeMemory = reinterpret_cast<NativeModule::FreeFn>(dlsym(module.handle, "rqm_free"));
#endif
    if (module.invoke == nullptr || module.freeMemory == nullptr) {
        throw std::runtime_error("Native module is missing the RayQuiro ABI exports: " + loadedFrom.string());
    }

    modules[moduleName] = module;
    return modules[moduleName];
}

inline Value native_call(const std::string& builtin, const std::vector<Value>& args) {
    const size_t dot = builtin.find('.');
    if (dot == std::string::npos) {
        throw std::runtime_error("Invalid native call target: " + builtin);
    }

    const std::string moduleName = builtin.substr(0, dot);
    const std::string functionName = builtin.substr(dot + 1);
    if (functionName.empty()) {
        throw std::runtime_error("Native call target is not a loadable module: " + builtin);
    }

    if (is_known_builtin_namespace(moduleName)) {
        if (!is_overridable_builtin_namespace(moduleName) || !has_native_module(moduleName)) {
            throw std::runtime_error("Native call target is not a loadable module: " + builtin);
        }
    }

    NativeModule& module = load_native_module(moduleName);
    char* resultJson = nullptr;
    char* errorMessage = nullptr;
    const std::string payload = json_stringify_value(array(args));
    const int code = module.invoke(functionName.c_str(), payload.c_str(), &resultJson, &errorMessage);

    std::string resultText = resultJson ? std::string(resultJson) : "null";
    std::string errorText = errorMessage ? std::string(errorMessage) : "";
    if (resultJson) module.freeMemory(resultJson);
    if (errorMessage) module.freeMemory(errorMessage);

    if (code != 0) {
        throw std::runtime_error(errorText.empty()
            ? ("Native module call failed: " + builtin)
            : ("Native module call failed: " + builtin + " :: " + errorText));
    }

    return json_parse({Value(resultText)});
}

inline std::optional<Value> maybe_native_builtin_override(const std::string& builtin, const std::vector<Value>& args) {
    const size_t dot = builtin.find('.');
    if (dot == std::string::npos) {
        return std::nullopt;
    }

    const std::string moduleName = builtin.substr(0, dot);
    if (!is_overridable_builtin_namespace(moduleName) || !has_native_module(moduleName)) {
        return std::nullopt;
    }

    try {
        return native_call(builtin, args);
    } catch (const std::exception& error) {
        if (is_native_fallback_error(error.what())) {
            return std::nullopt;
        }
        throw;
    }
}

inline std::mt19937& random_engine() {
    static std::mt19937 engine(static_cast<unsigned int>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    return engine;
}

inline Value random_value(const std::vector<Value>& args) {
    double min = 0.0;
    double max = 1.0;
    if (args.size() == 1) {
        max = to_number(args[0]);
    } else if (args.size() >= 2) {
        min = to_number(args[0]);
        max = to_number(args[1]);
    }
    if (max < min) std::swap(min, max);
    std::uniform_real_distribution<double> distribution(min, max);
    return Value(distribution(random_engine()));
}

inline Value random_int(const std::vector<Value>& args) {
    int min = 0;
    int max = 100;
    if (args.size() == 1) {
        max = static_cast<int>(to_number(args[0]));
    } else if (args.size() >= 2) {
        min = static_cast<int>(to_number(args[0]));
        max = static_cast<int>(to_number(args[1]));
    }
    if (max < min) std::swap(min, max);
    std::uniform_int_distribution<int> distribution(min, max);
    return Value(static_cast<double>(distribution(random_engine())));
}
}

)";
    }

    static void emitFsRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline Value fs_exists(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    return Value(std::filesystem::exists(std::filesystem::path(to_string(args[0]))));
}

inline Value fs_mkdir(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    try {
        std::filesystem::create_directories(std::filesystem::path(to_string(args[0])));
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}

inline Value fs_copy(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);

    try {
        const std::filesystem::path source = std::filesystem::path(to_string(args[0]));
        const std::filesystem::path target = std::filesystem::path(to_string(args[1]));
        const std::filesystem::path parent = target.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}

inline Value fs_copy_tree(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);

    try {
        const std::filesystem::path source = std::filesystem::path(to_string(args[0]));
        const std::filesystem::path target = std::filesystem::path(to_string(args[1]));
        std::filesystem::create_directories(target);
        std::filesystem::copy(
            source,
            target,
            std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}

inline Value fs_remove(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);

    try {
        const auto removed = std::filesystem::remove_all(std::filesystem::path(to_string(args[0])));
        return Value(static_cast<double>(removed));
    } catch (...) {
        return Value(0.0);
    }
}

inline Value fs_read(const std::vector<Value>& args) {
    if (args.empty()) return Value("");

    try {
        std::ifstream input(std::filesystem::path(to_string(args[0])), std::ios::binary);
        if (!input) return Value("");
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return Value(buffer.str());
    } catch (...) {
        return Value("");
    }
}

inline Value fs_write(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);

    try {
        const std::filesystem::path target = std::filesystem::path(to_string(args[0]));
        const std::filesystem::path parent = target.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        if (!output) return Value(false);
        output << to_string(args[1]);
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}
}

)";
    }

    static void emitProcessRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline Value process_run(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    const int code = std::system(to_string(args[0]).c_str());
    return Value(static_cast<double>(code));
}

inline Value process_exe_dir(const std::vector<Value>& args) {
    (void)args;
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD size = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (size == 0) return Value(std::filesystem::current_path().string());
    return Value(std::filesystem::path(std::string(buffer, size)).parent_path().string());
#else
    return Value(std::filesystem::current_path().string());
#endif
}
}

)";
    }

    static void emitEnvRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline std::string env_lower_copy(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

inline bool env_path_contains_segment(const std::string& pathValue, const std::string& candidate) {
    const std::string wanted = env_lower_copy(candidate);
    size_t start = 0;
    while (start <= pathValue.size()) {
        const size_t end = pathValue.find(';', start);
        const std::string item = pathValue.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty() && env_lower_copy(item) == wanted) {
            return true;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

inline Value env_get(const std::vector<Value>& args) {
    if (args.empty()) return Value("");
    const std::string name = to_string(args[0]);
    const char* value = std::getenv(name.c_str());
    return Value(value ? std::string(value) : std::string());
}

inline Value env_set(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);
    const std::string name = to_string(args[0]);
    const std::string value = to_string(args[1]);
#ifdef _WIN32
    return Value(_putenv_s(name.c_str(), value.c_str()) == 0);
#else
    return Value(setenv(name.c_str(), value.c_str(), 1) == 0);
#endif
}

inline Value env_path_add(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    const std::string candidate = std::filesystem::path(to_string(args[0])).lexically_normal().string();

#ifdef _WIN32
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_SET_VALUE, &key) != 0L) {
        return Value(false);
    }

    DWORD type = REG_EXPAND_SZ;
    DWORD size = 0;
    std::string currentValue;
    const LONG queryResult = RegQueryValueExA(key, "Path", nullptr, &type, nullptr, &size);
    if (queryResult == 0L && size > 1) {
        std::vector<char> buffer(size + 1, '\0');
        if (RegQueryValueExA(key, "Path", nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) == 0L) {
            currentValue.assign(buffer.data());
        }
    }

    if (!env_path_contains_segment(currentValue, candidate)) {
        const std::string updatedValue = currentValue.empty() ? candidate : currentValue + ";" + candidate;
        if (RegSetValueExA(
                key,
                "Path",
                0,
                REG_EXPAND_SZ,
                reinterpret_cast<const BYTE*>(updatedValue.c_str()),
                static_cast<DWORD>(updatedValue.size() + 1)) != 0L) {
            RegCloseKey(key);
            return Value(false);
        }
    }

    RegCloseKey(key);

    const char* processPath = std::getenv("PATH");
    std::string currentProcessPath = processPath ? processPath : "";
    if (!env_path_contains_segment(currentProcessPath, candidate)) {
        currentProcessPath = currentProcessPath.empty() ? candidate : currentProcessPath + ";" + candidate;
        _putenv_s("PATH", currentProcessPath.c_str());
    }

    return Value(true);
#else
    return Value(false);
#endif
}
}

)";
    }

    static void emitOsRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline Value os_name(const std::vector<Value>& args) {
    (void)args;
#ifdef _WIN32
    return Value("windows");
#elif defined(__APPLE__)
    return Value("macos");
#elif defined(__linux__)
    return Value("linux");
#else
    return Value("unknown");
#endif
}

inline Value os_arch(const std::vector<Value>& args) {
    (void)args;
#if defined(_M_X64) || defined(__x86_64__)
    return Value("x86_64");
#elif defined(_M_IX86) || defined(__i386__)
    return Value("x86");
#elif defined(_M_ARM64) || defined(__aarch64__)
    return Value("arm64");
#elif defined(_M_ARM) || defined(__arm__)
    return Value("arm");
#else
    return Value("unknown");
#endif
}

inline Value os_cwd(const std::vector<Value>& args) {
    (void)args;
    return Value(std::filesystem::current_path().string());
}

inline Value os_chdir(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    try {
        std::filesystem::current_path(std::filesystem::path(to_string(args[0])));
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}

inline Value os_home(const std::vector<Value>& args) {
    (void)args;
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return Value(home ? std::string(home) : std::string());
}

inline Value os_temp(const std::vector<Value>& args) {
    (void)args;
    return Value(std::filesystem::temp_directory_path().string());
}

inline Value os_sep(const std::vector<Value>& args) {
    (void)args;
    return Value(std::string(1, std::filesystem::path::preferred_separator));
}

inline Value os_exists(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    return Value(std::filesystem::exists(std::filesystem::path(to_string(args[0]))));
}

inline Value os_is_dir(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    const std::filesystem::path path = std::filesystem::path(to_string(args[0]));
    return Value(std::filesystem::exists(path) && std::filesystem::is_directory(path));
}

inline Value os_is_file(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    const std::filesystem::path path = std::filesystem::path(to_string(args[0]));
    return Value(std::filesystem::exists(path) && std::filesystem::is_regular_file(path));
}
}

)";
    }

    static void emitNetRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline std::unordered_map<int, SOCKET> net_connections;
inline int net_next_handle = 1;

inline SOCKET net_connect_ip(const std::string& host, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    if (host == "localhost") {
        address.sin_addr.s_addr = htonl(0x7F000001u);
    } else if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    address.sin_port = htons(static_cast<u_short>(port));
    if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

inline int net_store(SOCKET socketValue) {
    if (socketValue == INVALID_SOCKET) return 0;
    const int handle = net_next_handle++;
    net_connections[handle] = socketValue;
    return handle;
}

inline SOCKET net_take(int handle) {
    const auto found = net_connections.find(handle);
    return found == net_connections.end() ? INVALID_SOCKET : found->second;
}

inline Value net_tcp_connect(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(0.0);
    const std::string host = to_string(args[0]);
    const int port = static_cast<int>(to_number(args[1]));
    return Value(static_cast<double>(net_store(net_connect_ip(host, port))));
}

inline Value net_tcp_send(const std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);
    const SOCKET sock = net_take(static_cast<int>(to_number(args[0])));
    if (sock == INVALID_SOCKET) return Value(false);
    const std::string text = to_string(args[1]);
    return Value(send(sock, text.c_str(), static_cast<int>(text.size()), 0) != SOCKET_ERROR);
}

inline Value net_tcp_recv(const std::vector<Value>& args) {
    if (args.empty()) return Value("");
    const SOCKET sock = net_take(static_cast<int>(to_number(args[0])));
    if (sock == INVALID_SOCKET) return Value("");
    const int maxBytes = args.size() > 1 ? std::max(1, static_cast<int>(to_number(args[1]))) : 65536;
    std::string result;
    result.resize(static_cast<size_t>(maxBytes));
    const int received = recv(sock, result.data(), maxBytes, 0);
    if (received <= 0) return Value("");
    result.resize(static_cast<size_t>(received));
    return Value(result);
}

inline Value net_tcp_close(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    const int handle = static_cast<int>(to_number(args[0]));
    const auto found = net_connections.find(handle);
    if (found == net_connections.end()) return Value(false);
    closesocket(found->second);
    net_connections.erase(found);
    return Value(true);
}
}

)";
    }

    static void emitHttpRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline bool http_parse_url(const std::string& url, std::string& host, int& port, std::string& path) {
    const std::string scheme = "http://";
    if (url.rfind(scheme, 0) != 0) return false;
    const std::string remainder = url.substr(scheme.size());
    const size_t slash = remainder.find('/');
    const std::string hostPort = slash == std::string::npos ? remainder : remainder.substr(0, slash);
    path = slash == std::string::npos ? "/" : remainder.substr(slash);
    const size_t colon = hostPort.find(':');
    host = colon == std::string::npos ? hostPort : hostPort.substr(0, colon);
    port = colon == std::string::npos ? 80 : std::atoi(hostPort.substr(colon + 1).c_str());
    return !host.empty() && port > 0;
}

inline std::string http_body_from_response(const std::string& response) {
    const size_t headerEnd = response.find("\r\n\r\n");
    return headerEnd == std::string::npos ? response : response.substr(headerEnd + 4);
}

inline Value http_request(const std::string& method, const std::vector<Value>& args) {
    if (args.empty()) return Value::object();
    std::string host;
    int port = 80;
    std::string path;
    if (!http_parse_url(to_string(args[0]), host, port, path)) return Value::object();
    const std::string body = args.size() > 1 ? to_string(args[1]) : "";
    const SOCKET sock = net_connect_ip(host, port);
    if (sock == INVALID_SOCKET) return Value::object();
    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "Connection: close\r\n";
    if (method == "POST") {
        request << "Content-Type: text/plain; charset=utf-8\r\n";
        request << "Content-Length: " << body.size() << "\r\n";
    }
    request << "\r\n";
    if (method == "POST") request << body;
    const std::string payload = request.str();
    if (send(sock, payload.c_str(), static_cast<int>(payload.size()), 0) == SOCKET_ERROR) {
        closesocket(sock);
        return Value::object();
    }
    std::string response;
    char buffer[4096];
    while (true) {
        const int received = recv(sock, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        response.append(buffer, buffer + received);
    }
    closesocket(sock);

    int status = 0;
    std::istringstream stream(response);
    std::string httpVersion;
    stream >> httpVersion >> status;
    Value result = Value::object();
    (*result.as_object())["status"] = Value(static_cast<double>(status));
    (*result.as_object())["body"] = Value(http_body_from_response(response));
    return result;
}

inline Value http_get(const std::vector<Value>& args) {
    return http_request("GET", args);
}

inline Value http_post(const std::vector<Value>& args) {
    return http_request("POST", args);
}
}

)";
    }

    static void emitDbRuntime(std::ostream& out) {
        out << R"(namespace rq {
struct PgApi {
    bool loaded = false;
    bool available = false;
    void* library = nullptr;
    using ConnectDbFn = void* (*)(const char*);
    using ExecFn = void* (*)(void*, const char*);
    using FinishFn = void (*)(void*);
    using ResultStatusFn = int (*)(const void*);
    using NtuplesFn = int (*)(const void*);
    using NfieldsFn = int (*)(const void*);
    using FnameFn = const char* (*)(const void*, int);
    using GetvalueFn = const char* (*)(const void*, int, int);
    using ClearFn = void (*)(void*);
    using CmdTuplesFn = const char* (*)(const void*);
    ConnectDbFn connectDb = nullptr;
    ExecFn exec = nullptr;
    FinishFn finish = nullptr;
    ResultStatusFn resultStatus = nullptr;
    NtuplesFn ntuples = nullptr;
    NfieldsFn nfields = nullptr;
    FnameFn fname = nullptr;
    GetvalueFn getvalue = nullptr;
    ClearFn clear = nullptr;
    CmdTuplesFn cmdTuples = nullptr;
};

inline PgApi db_api;
inline std::unordered_map<int, void*> db_connections;
inline int db_next_handle = 1;

inline bool db_load() {
    if (db_api.loaded) return db_api.available;
#ifdef _WIN32
    db_api.library = LoadLibraryA("libpq.dll");
    auto proc = [&](const char* name) -> void* { return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(db_api.library), name)); };
#else
    db_api.library = dlopen("libpq.so.5", RTLD_NOW);
    if (!db_api.library) db_api.library = dlopen("libpq.so", RTLD_NOW);
    auto proc = [&](const char* name) -> void* { return dlsym(db_api.library, name); };
#endif
    if (!db_api.library) { db_api.loaded = true; db_api.available = false; return false; }
    db_api.connectDb = reinterpret_cast<PgApi::ConnectDbFn>(proc("PQconnectdb"));
    db_api.exec = reinterpret_cast<PgApi::ExecFn>(proc("PQexec"));
    db_api.finish = reinterpret_cast<PgApi::FinishFn>(proc("PQfinish"));
    db_api.resultStatus = reinterpret_cast<PgApi::ResultStatusFn>(proc("PQresultStatus"));
    db_api.ntuples = reinterpret_cast<PgApi::NtuplesFn>(proc("PQntuples"));
    db_api.nfields = reinterpret_cast<PgApi::NfieldsFn>(proc("PQnfields"));
    db_api.fname = reinterpret_cast<PgApi::FnameFn>(proc("PQfname"));
    db_api.getvalue = reinterpret_cast<PgApi::GetvalueFn>(proc("PQgetvalue"));
    db_api.clear = reinterpret_cast<PgApi::ClearFn>(proc("PQclear"));
    db_api.cmdTuples = reinterpret_cast<PgApi::CmdTuplesFn>(proc("PQcmdTuples"));
    db_api.available = db_api.connectDb && db_api.exec && db_api.finish && db_api.resultStatus && db_api.ntuples && db_api.nfields && db_api.fname && db_api.getvalue && db_api.clear && db_api.cmdTuples;
    db_api.loaded = true;
    return db_api.available;
}

inline Value db_connect(const std::vector<Value>& args) {
    if (args.empty()) return Value(0.0);
    if (!db_load()) return Value(0.0);
    void* connection = db_api.connectDb(to_string(args[0]).c_str());
    if (!connection) return Value(0.0);
    const int handle = db_next_handle++;
    db_connections[handle] = connection;
    return Value(static_cast<double>(handle));
}

inline void* db_take(int handle) {
    const auto found = db_connections.find(handle);
    return found == db_connections.end() ? nullptr : found->second;
}

inline Value db_query(const std::vector<Value>& args) {
    if (args.size() < 2 || !db_load()) return array({});
    void* connection = db_take(static_cast<int>(to_number(args[0])));
    if (!connection) return array({});
    void* result = db_api.exec(connection, to_string(args[1]).c_str());
    if (!result) return array({});
    const int status = db_api.resultStatus(result);
    if (!(status == 1 || status == 2)) { db_api.clear(result); return array({}); }
    const int rows = db_api.ntuples(result);
    const int cols = db_api.nfields(result);
    std::vector<Value> output;
    for (int row = 0; row < rows; ++row) {
        Value rowValue = Value::object();
        auto rowObj = rowValue.as_object();
        for (int col = 0; col < cols; ++col) {
            const char* key = db_api.fname(result, col);
            const char* cell = db_api.getvalue(result, row, col);
            (*rowObj)[key ? key : "column"] = Value(cell ? std::string(cell) : std::string());
        }
        output.push_back(rowValue);
    }
    db_api.clear(result);
    return array(output);
}

inline Value db_exec(const std::vector<Value>& args) {
    if (args.size() < 2 || !db_load()) return Value(0.0);
    void* connection = db_take(static_cast<int>(to_number(args[0])));
    if (!connection) return Value(0.0);
    void* result = db_api.exec(connection, to_string(args[1]).c_str());
    if (!result) return Value(0.0);
    const int status = db_api.resultStatus(result);
    if (!(status == 1 || status == 2)) { db_api.clear(result); return Value(0.0); }
    const char* tuples = db_api.cmdTuples(result);
    const double count = tuples && *tuples ? std::atof(tuples) : 0.0;
    db_api.clear(result);
    return Value(count);
}

inline Value db_scalar(const std::vector<Value>& args) {
    const Value rows = db_query(args);
    if (!is_array(rows) || rows.as_array()->empty()) return Value();
    const Value first = (*rows.as_array())[0];
    if (!is_object(first) || first.as_object()->empty()) return Value();
    return first.as_object()->begin()->second;
}

inline Value db_close(const std::vector<Value>& args) {
    if (args.empty() || !db_load()) return Value(false);
    const int handle = static_cast<int>(to_number(args[0]));
    const auto found = db_connections.find(handle);
    if (found == db_connections.end()) return Value(false);
    db_api.finish(found->second);
    db_connections.erase(found);
    return Value(true);
}
}

)";
    }

    static void emitUiRuntime(std::ostream& out) {
        out << R"(RayQuiroModernUI rayquiro_ui;

namespace rq {
inline Value ui_init(const std::vector<Value>& args) {
    const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const int width = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 920;
    const int height = args.size() > 2 ? static_cast<int>(to_number(args[2])) : 620;
    rayquiro_ui.init(title, width, height);
    return Value();
}

inline Value ui_style(const std::vector<Value>& args) {
    if (args.empty()) return Value();
    rayquiro_ui.style(to_string(args[0]));
    return Value();
}

inline Value ui_hero(const std::vector<Value>& args) {
    const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const std::string subtitle = args.size() > 1 ? to_string(args[1]) : "";
    rayquiro_ui.hero(title, subtitle);
    return Value();
}

inline Value ui_status(const std::vector<Value>& args) {
    const std::string title = args.size() > 0 ? to_string(args[0]) : "";
    const std::string body = args.size() > 1 ? to_string(args[1]) : "";
    rayquiro_ui.status(title, body);
    return Value();
}

inline Value ui_info(const std::vector<Value>& args) {
    const std::string label = args.size() > 0 ? to_string(args[0]) : "";
    const std::string value = args.size() > 1 ? to_string(args[1]) : "";
    rayquiro_ui.info(label, value);
    return Value();
}

inline Value ui_text(const std::vector<Value>& args) {
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string role = args.size() > 1 ? to_string(args[1]) : "body";
    rayquiro_ui.text(text, role);
    return Value();
}

inline Value ui_action(const std::vector<Value>& args) {
    if (args.size() < 2) return Value();
    const std::string variant = args.size() > 2 ? to_string(args[2]) : "primary";
    rayquiro_ui.action(to_string(args[0]), to_string(args[1]), variant);
    return Value();
}

inline Value ui_run(const std::vector<Value>& args) {
    (void)args;
    return Value(rayquiro_ui.run());
}
}

)";
    }

    static void emitWebRuntime(std::ostream& out) {
        out << R"RQ(namespace rq {
struct RayQuiroWebRoute {
    std::string route = "/";
    std::string title = "RayQuiro";
    std::string path;
    std::string head;
    std::string body;
    std::vector<std::string> stack;
};

struct RayQuiroWebBuilderState {
    std::string defaultTitle = "RayQuiro";
    std::string outputRoot = "build";
    std::string publicDir = "public";
    std::string bindHost = "127.0.0.1";
    std::string css;
    std::unordered_map<std::string, RayQuiroWebRoute> routes;
    std::string currentRoute = "/";
    bool active = false;
    bool liveMode = false;
    int livePort = 5274;
};

inline RayQuiroWebBuilderState& web_state() {
    static RayQuiroWebBuilderState state;
    return state;
}

inline std::string web_default_css() {
    std::string css;
    css += "body{margin:0;background:linear-gradient(160deg,#edf4ff,#f6fbff);color:#10243a;font-family:'Segoe UI',sans-serif;}";
    css += ".page{max-width:1080px;margin:0 auto;padding:56px 24px 72px;}";
    css += ".hero{font-size:52px;line-height:1.05;margin:0 0 14px;font-weight:800;letter-spacing:-0.03em;}";
    css += ".lead{font-size:20px;line-height:1.7;color:#49617e;margin:0 0 24px;}";
    css += ".panel{background:white;border:1px solid #d8e6f4;border-radius:24px;padding:28px;box-shadow:0 22px 60px rgba(16,36,58,.08);}";
    css += ".button{display:inline-flex;align-items:center;justify-content:center;padding:12px 18px;border-radius:16px;background:#0ea5e9;color:white;text-decoration:none;font-weight:700;border:none;}";
    return css;
}

inline std::string web_trim(std::string value) {
    const auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

inline std::string web_class_attr(const std::string& value) {
    if (value.empty()) return "";
    return " class=\"" + value + "\"";
}

inline std::string web_normalize_route(const std::string& rawValue) {
    std::string route = web_trim(rawValue);
    if (route.empty()) return "/";
    if (route[0] != '/') route = "/" + route;
    while (route.size() > 1 && route.back() == '/') route.pop_back();
    return route;
}

inline std::string web_route_to_output_path(const RayQuiroWebBuilderState& state, const std::string& routeValue) {
    const std::string route = web_normalize_route(routeValue);
    const std::filesystem::path root = state.outputRoot.empty() ? std::filesystem::path("build") : std::filesystem::path(state.outputRoot);
    if (route == "/") {
        return (root / "index.html").string();
    }
    return (root / route.substr(1) / "index.html").string();
}

inline std::string web_file_version(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::exists(path)) return "0";
        const auto stamp = std::filesystem::last_write_time(path).time_since_epoch().count();
        return std::to_string(static_cast<long long>(stamp));
    } catch (...) {
        return "0";
    }
}

inline std::string web_live_script() {
    return R"HTML(<script>
(function () {
  let current = null;
  async function poll() {
    try {
      const response = await fetch('/__rq_version', { cache: 'no-store' });
      const next = await response.text();
      if (current === null) {
        current = next;
      } else if (next !== current) {
        location.reload();
        return;
      }
    } catch (err) {}
    setTimeout(poll, 900);
  }
  poll();
})();
</script>)HTML";
}

inline std::string web_read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline void web_replace_all(std::string& text, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) return;
    size_t start = 0;
    while ((start = text.find(needle, start)) != std::string::npos) {
        text.replace(start, needle.size(), replacement);
        start += replacement.size();
    }
}

inline std::filesystem::path web_template_path(const RayQuiroWebBuilderState& state) {
    const std::filesystem::path candidate = std::filesystem::path(state.publicDir) / "index.html";
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
        return candidate;
    }
    return {};
}

inline RayQuiroWebRoute& web_current_route() {
    auto& state = web_state();
    const std::string key = state.currentRoute.empty() ? std::string("/") : state.currentRoute;
    auto found = state.routes.find(key);
    if (found == state.routes.end()) {
        RayQuiroWebRoute route;
        route.route = key;
        route.title = state.defaultTitle;
        route.path = web_route_to_output_path(state, key);
        found = state.routes.insert({key, route}).first;
    }
    return found->second;
}

inline void web_prepare_session() {
    auto& state = web_state();
    if (state.defaultTitle.empty()) state.defaultTitle = "RayQuiro";
    if (state.outputRoot.empty()) state.outputRoot = "build";
    if (state.publicDir.empty()) state.publicDir = "public";
    if (state.css.empty()) state.css = web_default_css();
    if (state.currentRoute.empty()) state.currentRoute = "/";
    state.active = true;

    auto& route = web_current_route();
    if (route.route.empty()) route.route = "/";
    if (route.title.empty()) route.title = state.defaultTitle;
    if (route.path.empty()) route.path = web_route_to_output_path(state, route.route);
}

inline void web_push_tag(const std::string& tag) {
    web_current_route().stack.push_back(tag);
}

inline std::string web_pop_tag(const std::string& fallback) {
    auto& route = web_current_route();
    if (route.stack.empty()) return fallback;
    const std::string tag = route.stack.back();
    route.stack.pop_back();
    return tag;
}

inline std::string web_extract_tag_name(const std::string& markup) {
    const std::string trimmed = web_trim(markup);
    if (trimmed.size() < 3 || trimmed[0] != '<' || trimmed[1] == '/' || trimmed[1] == '!') return "";
    size_t start = 1;
    while (start < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[start])) != 0) ++start;
    size_t end = start;
    while (end < trimmed.size()) {
        const char ch = trimmed[end];
        if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '>' || ch == '/') break;
        ++end;
    }
    if (end <= start) return "";
    return trimmed.substr(start, end - start);
}

inline bool web_should_push_markup_tag(const std::string& markup) {
    const std::string trimmed = web_trim(markup);
    if (trimmed.empty() || trimmed[0] != '<') return false;
    if (trimmed.size() >= 2 && trimmed[1] == '/') return false;
    if (trimmed.find("/>") != std::string::npos) return false;
    if (trimmed.find("</") != std::string::npos) return false;
    return !web_extract_tag_name(trimmed).empty();
}

inline void web_copy_public_assets(const RayQuiroWebBuilderState& state) {
    const std::filesystem::path publicRoot(state.publicDir);
    if (!std::filesystem::exists(publicRoot) || !std::filesystem::is_directory(publicRoot)) {
        return;
    }

    const std::filesystem::path outputRoot(state.outputRoot);
    std::filesystem::create_directories(outputRoot);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot)) {
        if (!entry.is_regular_file()) continue;
        const std::filesystem::path relative = std::filesystem::relative(entry.path(), publicRoot);
        const std::filesystem::path target = outputRoot / relative;
        const auto parent = target.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing);
    }
}

inline std::string web_render_document(const RayQuiroWebBuilderState& state, const RayQuiroWebRoute& route) {
    const std::string titleBlock = "<title>" + route.title + "</title>";
    const std::string styleBlock = "<style>" + state.css + "</style>";
    const std::string liveBlock = state.liveMode ? web_live_script() : "";
    const std::string headBlock = route.head;

    const std::filesystem::path templatePath = web_template_path(state);
    if (!templatePath.empty()) {
        std::string html = web_read_file(templatePath);
        web_replace_all(html, "{{ rq_title }}", route.title);
        web_replace_all(html, "<!-- rq-styles -->", styleBlock);
        web_replace_all(html, "<!-- rq-head -->", titleBlock + styleBlock + headBlock);
        web_replace_all(html, "<!-- rq-body -->", route.body);
        web_replace_all(html, "<!-- rq-live -->", liveBlock);

        if (html.find(titleBlock) == std::string::npos && html.find("</head>") != std::string::npos) {
            html.insert(html.find("</head>"), titleBlock + styleBlock + headBlock);
        }
        if (html.find(route.body) == std::string::npos && html.find("</body>") != std::string::npos) {
            html.insert(html.find("</body>"), route.body + liveBlock);
        }
        return html;
    }

    std::string html = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += titleBlock;
    html += styleBlock;
    html += headBlock;
    html += "</head><body>";
    html += route.body;
    html += liveBlock;
    html += "</body></html>";
    return html;
}

inline void web_write_output(const RayQuiroWebBuilderState& state) {
    web_copy_public_assets(state);
    for (const auto& pair : state.routes) {
        const RayQuiroWebRoute& route = pair.second;
        const std::filesystem::path outPath(route.path);
        const auto parent = outPath.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::ofstream file(outPath, std::ios::binary | std::ios::trunc);
        file << web_render_document(state, route);
    }
}

#ifdef _WIN32
inline std::string web_content_type_for(const std::filesystem::path& requested) {
    const std::string extension = requested.extension().string();
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "application/javascript; charset=utf-8";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".ico") return "image/x-icon";
    return "text/html; charset=utf-8";
}

inline void web_send_http(SOCKET client, const std::string& status, const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << body;
    const std::string payload = response.str();
    send(client, payload.c_str(), static_cast<int>(payload.size()), 0);
}

inline std::optional<std::filesystem::path> web_route_request_target(const RayQuiroWebBuilderState& state, const std::string& requestPath) {
    std::string path = requestPath;
    const size_t query = path.find('?');
    if (query != std::string::npos) path = path.substr(0, query);
    const std::string normalized = web_normalize_route(path);
    const auto found = state.routes.find(normalized);
    if (found == state.routes.end()) return std::nullopt;
    return std::filesystem::path(found->second.path);
}

inline std::optional<std::filesystem::path> web_static_request_target(const RayQuiroWebBuilderState& state, const std::string& requestPath) {
    std::string path = requestPath;
    const size_t query = path.find('?');
    if (query != std::string::npos) path = path.substr(0, query);
    if (path.empty() || path == "/") return std::nullopt;
    std::string relative = path[0] == '/' ? path.substr(1) : path;
    if (relative.find("..") != std::string::npos) return std::nullopt;
    const std::filesystem::path requested = std::filesystem::path(state.outputRoot) / relative;
    if (std::filesystem::exists(requested) && std::filesystem::is_regular_file(requested)) {
        return requested;
    }
    return std::nullopt;
}

inline void web_handle_client(SOCKET client, const RayQuiroWebBuilderState& state) {
    char buffer[4096];
    const int received = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) return;
    buffer[received] = '\0';

    std::istringstream request(std::string(buffer, received));
    std::string method;
    std::string path;
    request >> method >> path;

    const std::filesystem::path versionPath = std::filesystem::path(state.outputRoot) / "index.html";
    if (path == "/__rq_version") {
        web_send_http(client, "200 OK", "text/plain; charset=utf-8", web_file_version(versionPath));
        return;
    }

    if (const auto routeTarget = web_route_request_target(state, path)) {
        web_send_http(client, "200 OK", "text/html; charset=utf-8", web_read_file(*routeTarget));
        return;
    }

    if (const auto staticTarget = web_static_request_target(state, path)) {
        web_send_http(client, "200 OK", web_content_type_for(*staticTarget), web_read_file(*staticTarget));
        return;
    }

    web_send_http(client, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
}

inline void web_serve_forever(const RayQuiroWebBuilderState& state) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "[RayQuiro] Failed to start Winsock for web.live()." << std::endl;
        return;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        WSACleanup();
        std::cerr << "[RayQuiro] Failed to create web server socket." << std::endl;
        return;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    if (state.bindHost.empty() || state.bindHost == "0.0.0.0" || state.bindHost == "*") {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, state.bindHost.c_str(), &address.sin_addr) != 1) {
        std::cerr << "[RayQuiro] Invalid bind host: " << state.bindHost << std::endl;
        closesocket(server);
        WSACleanup();
        return;
    }
    address.sin_port = htons(static_cast<u_short>(state.livePort));

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "[RayQuiro] Port " << state.livePort << " is busy." << std::endl;
        closesocket(server);
        WSACleanup();
        return;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[RayQuiro] Failed to listen on port " << state.livePort << "." << std::endl;
        closesocket(server);
        WSACleanup();
        return;
    }

    std::cout << "[RayQuiro] Live web server: http://127.0.0.1:" << state.livePort << std::endl;
    std::cout << "[RayQuiro] Press Ctrl+C to stop." << std::endl;

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            break;
        }
        web_handle_client(client, state);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
#else
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        std::cerr << "[RayQuiro] Failed to create web server socket." << std::endl;
        return;
    }

    const int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    if (state.bindHost.empty() || state.bindHost == "0.0.0.0" || state.bindHost == "*") {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, state.bindHost.c_str(), &address.sin_addr) != 1) {
        std::cerr << "[RayQuiro] Invalid bind host: " << state.bindHost << std::endl;
        closesocket(server);
        return;
    }
    address.sin_port = htons(static_cast<u_short>(state.livePort));

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "[RayQuiro] Port " << state.livePort << " is busy." << std::endl;
        closesocket(server);
        return;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[RayQuiro] Failed to listen on port " << state.livePort << "." << std::endl;
        closesocket(server);
        return;
    }

    std::cout << "[RayQuiro] Live web server: http://127.0.0.1:" << state.livePort << std::endl;
    std::cout << "[RayQuiro] Press Ctrl+C to stop." << std::endl;

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            break;
        }
        web_handle_client(client, state);
        closesocket(client);
    }

    closesocket(server);
#endif
}

inline Value web_begin(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.begin", args)) return *overrideResult;
    auto& state = web_state();
    state = RayQuiroWebBuilderState{};
    state.defaultTitle = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const std::string requestedPath = args.size() > 1 ? to_string(args[1]) : "build/index.html";
    state.css = web_default_css();
    state.outputRoot = std::filesystem::path(requestedPath).parent_path().empty()
        ? std::string("build")
        : std::filesystem::path(requestedPath).parent_path().string();
    state.currentRoute = "/";
    state.active = true;

    RayQuiroWebRoute route;
    route.route = "/";
    route.title = state.defaultTitle;
    route.path = requestedPath;
    state.routes["/"] = route;
    return Value();
}

inline Value web_route(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.route", args)) return *overrideResult;
    web_prepare_session();
    auto& state = web_state();
    const std::string routeKey = web_normalize_route(args.empty() ? "/" : to_string(args[0]));
    RayQuiroWebRoute& route = state.routes[routeKey];
    if (route.route.empty()) route.route = routeKey;
    if (route.title.empty()) route.title = state.defaultTitle;
    if (route.path.empty()) route.path = web_route_to_output_path(state, routeKey);
    if (args.size() > 1) route.title = to_string(args[1]);
    if (args.size() > 2) route.path = to_string(args[2]);
    state.currentRoute = routeKey;
    return Value(route.path);
}

inline Value web_head(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.head", args)) return *overrideResult;
    web_prepare_session();
    if (!args.empty()) {
        web_current_route().head += to_string(args[0]);
    }
    return Value();
}

inline Value web_public(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.public", args)) return *overrideResult;
    web_prepare_session();
    auto& state = web_state();
    state.publicDir = args.empty() ? "public" : to_string(args[0]);
    return Value(state.publicDir);
}

inline Value web_live(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.live", args)) return *overrideResult;
    web_prepare_session();
    auto& state = web_state();
    state.liveMode = true;
    state.livePort = args.empty() ? 5274 : static_cast<int>(to_number(args[0]));
    if (state.livePort <= 0) {
        state.livePort = 5274;
    }
    const char* defaultBind = std::getenv("RQIO_WEB_HOST");
    state.bindHost = args.size() > 1 ? to_string(args[1]) : (defaultBind ? defaultBind : "127.0.0.1");
    return Value(static_cast<double>(state.livePort));
}

inline Value web_style(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.style", args)) return *overrideResult;
    if (args.empty()) return Value();
    web_prepare_session();
    auto& state = web_state();
    if (!state.css.empty()) state.css += "\n";
    state.css += to_string(args[0]);
    return Value();
}

inline Value web_open(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.open", args)) return *overrideResult;
    web_prepare_session();
    auto& route = web_current_route();
    const std::string first = args.size() > 0 ? to_string(args[0]) : "div";
    const std::string trimmed = web_trim(first);
    if (!trimmed.empty() && trimmed[0] == '<') {
        route.body += first;
        if (web_should_push_markup_tag(trimmed)) {
            web_push_tag(web_extract_tag_name(trimmed));
        }
        return Value();
    }

    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    route.body += "<" + first + web_class_attr(className) + ">";
    web_push_tag(first);
    return Value();
}

inline Value web_close(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.close", args)) return *overrideResult;
    web_prepare_session();
    const std::string fallback = args.size() > 0 ? to_string(args[0]) : "div";
    web_current_route().body += "</" + web_pop_tag(fallback) + ">";
    return Value();
}

inline Value web_text(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.text", args)) return *overrideResult;
    web_prepare_session();
    auto& route = web_current_route();
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    const std::string tag = args.size() > 2 ? to_string(args[2]) : "span";
    route.body += "<" + tag + web_class_attr(className) + ">" + text + "</" + tag + ">";
    return Value();
}

inline Value web_h1(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.h1", args)) return *overrideResult;
    web_prepare_session();
    auto& route = web_current_route();
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    route.body += "<h1" + web_class_attr(className) + ">" + text + "</h1>";
    return Value();
}

inline Value web_h2(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.h2", args)) return *overrideResult;
    web_prepare_session();
    auto& route = web_current_route();
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    route.body += "<h2" + web_class_attr(className) + ">" + text + "</h2>";
    return Value();
}

inline Value web_p(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.p", args)) return *overrideResult;
    web_prepare_session();
    auto& route = web_current_route();
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    route.body += "<p" + web_class_attr(className) + ">" + text + "</p>";
    return Value();
}

inline Value web_button(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.button", args)) return *overrideResult;
    web_prepare_session();
    auto& route = web_current_route();
    const std::string label = args.size() > 0 ? to_string(args[0]) : "Button";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "button";
    const std::string href = args.size() > 2 ? to_string(args[2]) : "#";
    route.body += "<a href=\"" + href + "\"" + web_class_attr(className) + ">" + label + "</a>";
    return Value();
}

inline Value web_raw(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.raw", args)) return *overrideResult;
    if (args.empty()) return Value();
    web_prepare_session();
    web_current_route().body += to_string(args[0]);
    return Value();
}

inline Value web_end(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.end", args)) return *overrideResult;
    (void)args;
    auto& state = web_state();
    for (auto& pair : state.routes) {
        while (!pair.second.stack.empty()) {
            pair.second.body += "</" + pair.second.stack.back() + ">";
            pair.second.stack.pop_back();
        }
    }

    web_write_output(state);
    state.active = false;

    if (state.liveMode) {
#ifdef _WIN32
        web_serve_forever(state);
#else
        std::cout << "[RayQuiro] web.live() currently falls back to writing into " << state.outputRoot << " on this platform." << std::endl;
#endif
    }
    return Value();
}

inline Value web_page(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("web.page", args)) return *overrideResult;
    auto& state = web_state();
    state = RayQuiroWebBuilderState{};
    state.defaultTitle = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const std::string requestedPath = args.size() > 2 ? to_string(args[2]) : "build/index.html";
    state.css = args.size() > 3 ? to_string(args[3]) : web_default_css();
    state.outputRoot = std::filesystem::path(requestedPath).parent_path().empty()
        ? std::string("build")
        : std::filesystem::path(requestedPath).parent_path().string();
    state.currentRoute = "/";

    RayQuiroWebRoute route;
    route.route = "/";
    route.title = state.defaultTitle;
    route.path = requestedPath;
    route.body = args.size() > 1 ? to_string(args[1]) : "";
    state.routes["/"] = route;
    web_write_output(state);
    return Value();
}
}

)RQ";
    }

    static void emitAppRuntime(std::ostream& out) {
        out << R"(RayQuiroApp rayquiro_app;

namespace rq {
inline Value app_init(const std::vector<Value>& args) {
    const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const int width = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 900;
    const int height = args.size() > 2 ? static_cast<int>(to_number(args[2])) : 600;
    rayquiro_app.init(title, width, height);
    return Value();
}

inline Value app_run(const std::vector<Value>& args) {
    (void)args;
    rayquiro_app.run();
    return Value();
}

inline Value app_button(const std::vector<Value>& args) {
    if (args.size() < 5) return Value();
    rayquiro_app.add_button(
        to_string(args[0]),
        static_cast<int>(to_number(args[1])),
        static_cast<int>(to_number(args[2])),
        static_cast<int>(to_number(args[3])),
        static_cast<int>(to_number(args[4]))
    );
    return Value();
}

inline Value app_text(const std::vector<Value>& args) {
    if (args.size() < 5) return Value();
    rayquiro_app.add_text(
        to_string(args[0]),
        static_cast<int>(to_number(args[1])),
        static_cast<int>(to_number(args[2])),
        static_cast<int>(to_number(args[3])),
        static_cast<int>(to_number(args[4]))
    );
    return Value();
}

inline Value app_msg(const std::vector<Value>& args) {
    const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const std::string text = args.size() > 1 ? to_string(args[1]) : "";
    RayQuiroApp::show_message(title, text);
    return Value();
}
}

)";
    }

    static void emitEngineRuntime(std::ostream& out) {
        out << R"(namespace rq {
inline RTColor engine_color_from(const Value& value, RTColor fallback) {
    if (is_array(value)) {
        auto items = value.as_array();
        if (items->size() >= 3) {
            const unsigned char r = static_cast<unsigned char>(to_number((*items)[0]));
            const unsigned char g = static_cast<unsigned char>(to_number((*items)[1]));
            const unsigned char b = static_cast<unsigned char>(to_number((*items)[2]));
            const unsigned char a = items->size() >= 4 ? static_cast<unsigned char>(to_number((*items)[3])) : 255;
            return RTColor{r, g, b, a};
        }
    }
    return fallback;
}

inline RTVec3 engine_vec3_from(const Value& value, RTVec3 fallback) {
    if (is_array(value)) {
        auto items = value.as_array();
        if (items->size() >= 3) {
            return RTVec3{
                static_cast<float>(to_number((*items)[0])),
                static_cast<float>(to_number((*items)[1])),
                static_cast<float>(to_number((*items)[2]))
            };
        }
    }
    return fallback;
}

inline RTVec2 engine_vec2_from(const Value& value, RTVec2 fallback) {
    if (is_array(value)) {
        auto items = value.as_array();
        if (items->size() >= 2) {
            return RTVec2{
                static_cast<float>(to_number((*items)[0])),
                static_cast<float>(to_number((*items)[1]))
            };
        }
    }
    return fallback;
}

struct EngineLight {
    RTVec3 direction{-0.4f, -1.0f, -0.25f};
    RTColor color{255, 245, 222, 255};
    float intensity = 1.0f;
};

struct EngineMaterial {
    RTColor albedo{255, 255, 255, 255};
    RTColor emissive{0, 0, 0, 255};
    float roughness = 0.5f;
    float metallic = 0.0f;
    std::string texture;
    std::string normalTexture;
};

struct EngineMesh {
    std::string primitive = "cube";
    std::string source;
    RTVec3 defaultSize{1.0f, 1.0f, 1.0f};
    RTVec2 defaultPlaneSize{4.0f, 4.0f};
    float defaultRadius = 1.0f;
};

struct EngineTexture {
    std::string source;
    bool srgb = true;
    bool normalMap = false;
};

struct EngineEntity {
    std::string kind = "cube";
    RTVec3 position{0.0f, 0.5f, 0.0f};
    RTVec3 size{1.0f, 1.0f, 1.0f};
    RTVec3 scale{1.0f, 1.0f, 1.0f};
    RTVec2 planeSize{4.0f, 4.0f};
    float radius = 1.0f;
    RTColor color{255, 255, 255, 255};
    bool visible = true;
    std::string material;
    std::string mesh;
    std::string texture;
};

struct EngineScene {
    std::unordered_map<std::string, EngineEntity> entities;
    std::unordered_map<std::string, EngineMaterial> materials;
    std::unordered_map<std::string, EngineMesh> meshes;
    std::unordered_map<std::string, EngineTexture> textures;
    RTColor ambient{30, 38, 54, 255};
    EngineLight sun;
    bool hasSun = false;
};

inline std::string& engine_assets_root_value() {
    static std::string root = "assets";
    return root;
}

inline std::string& engine_current_scene_name() {
    static std::string name = "main";
    return name;
}

inline std::unordered_map<std::string, EngineScene>& engine_scenes() {
    static std::unordered_map<std::string, EngineScene> scenes;
    return scenes;
}

inline EngineScene& engine_current_scene() {
    auto& scenes = engine_scenes();
    auto found = scenes.find(engine_current_scene_name());
    if (found == scenes.end()) {
        found = scenes.emplace(engine_current_scene_name(), EngineScene{}).first;
    }
    return found->second;
}

inline unsigned char engine_clamp_channel(float value) {
    if (value < 0.0f) return 0;
    if (value > 255.0f) return 255;
    return static_cast<unsigned char>(value);
}

inline RTColor engine_apply_light(RTColor base, RTColor ambient, const std::optional<EngineLight>& light) {
    const float ambientFactor = 0.45f;
    const float lightFactor = light.has_value() ? std::max(0.0f, light->intensity) : 1.0f;
    const float sunMix = light.has_value() ? 0.55f : 0.35f;
    const RTColor lightColor = light.has_value() ? light->color : RTColor{255, 255, 255, 255};

    const float mixedR = base.r * (ambientFactor + sunMix * lightFactor * (lightColor.r / 255.0f));
    const float mixedG = base.g * (ambientFactor + sunMix * lightFactor * (lightColor.g / 255.0f));
    const float mixedB = base.b * (ambientFactor + sunMix * lightFactor * (lightColor.b / 255.0f));

    return RTColor{
        engine_clamp_channel(mixedR * 0.7f + ambient.r * 0.3f),
        engine_clamp_channel(mixedG * 0.7f + ambient.g * 0.3f),
        engine_clamp_channel(mixedB * 0.7f + ambient.b * 0.3f),
        base.a
    };
}

inline RTColor engine_mix_color(RTColor base, RTColor overlay, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    return RTColor{
        engine_clamp_channel(base.r * (1.0f - factor) + overlay.r * factor),
        engine_clamp_channel(base.g * (1.0f - factor) + overlay.g * factor),
        engine_clamp_channel(base.b * (1.0f - factor) + overlay.b * factor),
        base.a
    };
}

inline RTVec3 engine_mul_vec3(RTVec3 value, RTVec3 scale) {
    return RTVec3{value.x * scale.x, value.y * scale.y, value.z * scale.z};
}

inline EngineMaterial engine_material_from_args(const std::vector<Value>& args, size_t colorIndex) {
    EngineMaterial material;
    if (args.size() > colorIndex) {
        material.albedo = engine_color_from(args[colorIndex], material.albedo);
    }
    if (args.size() > colorIndex + 1) {
        material.roughness = static_cast<float>(to_number(args[colorIndex + 1]));
    }
    if (args.size() > colorIndex + 2) {
        material.metallic = static_cast<float>(to_number(args[colorIndex + 2]));
    }
    if (args.size() > colorIndex + 3) {
        material.emissive = engine_color_from(args[colorIndex + 3], material.emissive);
    }
    return material;
}

inline EngineMesh engine_mesh_from_args(const std::vector<Value>& args, size_t startIndex) {
    EngineMesh mesh;
    if (args.size() > startIndex) {
        mesh.primitive = to_string(args[startIndex]);
    }
    if (args.size() > startIndex + 1) {
        if (is_string(args[startIndex + 1])) {
            mesh.source = to_string(args[startIndex + 1]);
        } else if (is_array(args[startIndex + 1])) {
            mesh.defaultSize = engine_vec3_from(args[startIndex + 1], mesh.defaultSize);
            mesh.defaultPlaneSize = engine_vec2_from(args[startIndex + 1], mesh.defaultPlaneSize);
            auto items = args[startIndex + 1].as_array();
            if (!items->empty()) {
                mesh.defaultRadius = static_cast<float>(to_number((*items)[0]));
            }
        }
    }
    if (args.size() > startIndex + 2 && is_array(args[startIndex + 2])) {
        mesh.defaultSize = engine_vec3_from(args[startIndex + 2], mesh.defaultSize);
        mesh.defaultPlaneSize = engine_vec2_from(args[startIndex + 2], mesh.defaultPlaneSize);
        auto items = args[startIndex + 2].as_array();
        if (!items->empty()) {
            mesh.defaultRadius = static_cast<float>(to_number((*items)[0]));
        }
    }
    return mesh;
}

inline EngineTexture engine_texture_from_args(const std::vector<Value>& args, size_t startIndex) {
    EngineTexture texture;
    if (args.size() > startIndex) {
        texture.source = to_string(args[startIndex]);
    }
    if (args.size() > startIndex + 1) {
        texture.srgb = truthy(args[startIndex + 1]);
    }
    if (args.size() > startIndex + 2) {
        texture.normalMap = truthy(args[startIndex + 2]);
    }
    return texture;
}

inline Value engine_init(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.init", args)) return *overrideResult;
    const int width = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 1280;
    const int height = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 720;
    const std::string title = args.size() > 2 ? to_string(args[2]) : "Raytolfas Engine";
    rt_init(width, height, title.c_str());
    return Value();
}

inline Value engine_shutdown(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.shutdown", args)) return *overrideResult;
    (void)args;
    rt_shutdown();
    return Value();
}

inline Value engine_should_close(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.should_close", args)) return *overrideResult;
    (void)args;
    return Value(rt_should_close() != 0);
}

inline Value engine_begin(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.begin", args)) return *overrideResult;
    (void)args;
    rt_begin();
    return Value();
}

inline Value engine_end(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.end", args)) return *overrideResult;
    (void)args;
    rt_end();
    return Value();
}

inline Value engine_clear(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.clear", args)) return *overrideResult;
    RTColor color{11, 18, 28, 255};
    if (!args.empty()) {
        color = engine_color_from(args[0], color);
    }
    rt_clear(color);
    return Value();
}

inline Value engine_set_camera(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.set_camera", args)) return *overrideResult;
    RTVec3 position{6.0f, 6.0f, 6.0f};
    RTVec3 target{0.0f, 0.0f, 0.0f};
    RTVec3 up{0.0f, 1.0f, 0.0f};
    float fov = 45.0f;

    if (args.size() >= 1) position = engine_vec3_from(args[0], position);
    if (args.size() >= 2) target = engine_vec3_from(args[1], target);
    if (args.size() >= 3) up = engine_vec3_from(args[2], up);
    if (args.size() >= 4) fov = static_cast<float>(to_number(args[3]));

    rt_set_camera(position.x, position.y, position.z, target.x, target.y, target.z, up.x, up.y, up.z, fov);
    return Value();
}

inline Value engine_target_fps(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.target_fps", args)) return *overrideResult;
    const int fps = args.empty() ? 60 : static_cast<int>(to_number(args[0]));
    rt_set_target_fps(fps);
    return Value();
}

inline Value engine_frame_time(const std::vector<Value>& args) {
    (void)args;
    return Value(static_cast<double>(rt_get_frame_time()));
}

inline Value engine_backend(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.backend", args)) return *overrideResult;
    const std::string backend = args.empty() ? "raylib" : to_string(args[0]);
    rt_set_backend(backend.c_str());
    return Value(backend);
}

inline Value engine_backend_info(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.backend_info", args)) return *overrideResult;
    (void)args;
    return object({
        {"requested", Value(std::string(rt_backend_requested_name()))},
        {"active", Value(std::string(rt_backend_name()))},
        {"supports3d", Value(rt_backend_supports_3d() != 0)},
        {"available", Value(rt_backend_is_available() != 0)},
        {"placeholder", Value(rt_backend_is_placeholder() != 0)},
        {"vulkan_family", Value(rt_backend_is_vulkan_family() != 0)},
        {"gpu_count", Value(static_cast<double>(rt_backend_gpu_count()))},
        {"surface_ready", Value(rt_backend_surface_ready() != 0)},
        {"device_ready", Value(rt_backend_device_ready() != 0)},
        {"presentation_ready", Value(rt_backend_presentation_ready() != 0)},
        {"queue_family", Value(static_cast<double>(rt_backend_queue_family_index()))},
        {"swapchain_ready", Value(rt_backend_swapchain_ready() != 0)},
        {"swapchain_images", Value(static_cast<double>(rt_backend_swapchain_image_count()))},
        {"swapchain_width", Value(static_cast<double>(rt_backend_swapchain_width()))},
        {"swapchain_height", Value(static_cast<double>(rt_backend_swapchain_height()))},
        {"render_pass_ready", Value(rt_backend_render_pass_ready() != 0)},
        {"framebuffer_count", Value(static_cast<double>(rt_backend_framebuffer_count()))},
        {"depth_ready", Value(rt_backend_depth_ready() != 0)},
        {"geometry_buffers_ready", Value(rt_backend_geometry_buffers_ready() != 0)},
        {"vertex_buffer_bytes", Value(static_cast<double>(rt_backend_vertex_buffer_bytes()))},
        {"index_buffer_bytes", Value(static_cast<double>(rt_backend_index_buffer_bytes()))},
        {"shader_assets_ready", Value(rt_backend_shader_assets_ready() != 0)},
        {"shader_modules_ready", Value(rt_backend_shader_modules_ready() != 0)},
        {"pipeline_layout_ready", Value(rt_backend_pipeline_layout_ready() != 0)},
        {"texture_sampler_ready", Value(rt_backend_texture_sampler_ready() != 0)},
        {"texture_image_ready", Value(rt_backend_texture_image_ready() != 0)},
        {"descriptor_set_ready", Value(rt_backend_descriptor_set_ready() != 0)},
        {"graphics_pipeline_ready", Value(rt_backend_graphics_pipeline_ready() != 0)},
        {"command_pool_ready", Value(rt_backend_command_pool_ready() != 0)},
        {"command_buffer_count", Value(static_cast<double>(rt_backend_command_buffer_count()))},
        {"sync_ready", Value(rt_backend_sync_ready() != 0)},
        {"frame_path_ready", Value(rt_backend_frame_path_ready() != 0)},
        {"frame_acquired", Value(rt_backend_frame_acquired() != 0)},
        {"presented_frames", Value(static_cast<double>(rt_backend_presented_frame_count()))}
    });
}

inline Value engine_backend_name(const std::vector<Value>& args) {
    (void)args;
    return Value(std::string(rt_backend_name()));
}

inline Value engine_vsync(const std::vector<Value>& args) {
    if (args.empty()) return Value(rt_get_vsync() != 0);
    rt_set_vsync(truthy(args[0]) ? 1 : 0);
    return Value(rt_get_vsync() != 0);
}

inline Value engine_msaa(const std::vector<Value>& args) {
    if (args.empty()) return Value(static_cast<double>(rt_get_msaa()));
    rt_set_msaa(static_cast<int>(to_number(args[0])));
    return Value(static_cast<double>(rt_get_msaa()));
}

inline Value engine_exposure(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.exposure", args)) return *overrideResult;
    return args.empty() ? Value(1.0) : Value(to_number(args[0]));
}

inline Value engine_vignette(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.vignette", args)) return *overrideResult;
    return args.empty() ? Value(0.08) : Value(to_number(args[0]));
}

inline Value engine_film_grain(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.film_grain", args)) return *overrideResult;
    return args.empty() ? Value(0.02) : Value(to_number(args[0]));
}

inline Value engine_saturation(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.saturation", args)) return *overrideResult;
    return args.empty() ? Value(1.0) : Value(to_number(args[0]));
}

inline Value engine_contrast(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.contrast", args)) return *overrideResult;
    return args.empty() ? Value(1.0) : Value(to_number(args[0]));
}

inline Value engine_bloom(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.bloom", args)) return *overrideResult;
    return args.empty() ? Value(0.0) : Value(to_number(args[0]));
}

inline Value engine_fog(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.fog", args)) return *overrideResult;
    return Value();
}

inline Value engine_volumetric(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.volumetric", args)) return *overrideResult;
    return args.empty() ? Value(0.0) : Value(to_number(args[0]));
}

inline Value engine_postfx_info(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.postfx_info", args)) return *overrideResult;
    (void)args;
    return object({});
}

inline Value engine_scene_watch(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_watch", args)) return *overrideResult;
    (void)args;
    return Value(false);
}

inline Value engine_scene_reload(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_reload", args)) return *overrideResult;
    (void)args;
    return Value(false);
}

inline Value engine_stats(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.stats", args)) return *overrideResult;
    (void)args;
    auto& scenes = engine_scenes();
    auto& scene = engine_current_scene();
    return object({
        {"backend", Value(std::string(rt_backend_name()))},
        {"current_scene", Value(engine_current_scene_name())},
        {"scene_count", Value(static_cast<double>(scenes.size()))},
        {"entity_count", Value(static_cast<double>(scene.entities.size()))},
        {"material_count", Value(static_cast<double>(scene.materials.size()))},
        {"mesh_count", Value(static_cast<double>(scene.meshes.size()))},
        {"texture_count", Value(static_cast<double>(scene.textures.size()))},
        {"target_fps", Value(static_cast<double>(rt_get_target_fps()))},
        {"vsync", Value(rt_get_vsync() != 0)},
        {"msaa", Value(static_cast<double>(rt_get_msaa()))},
        {"camera_fov", Value(static_cast<double>(rt_get_camera_fov()))},
        {"draw_calls", Value(static_cast<double>(rt_get_draw_calls()))},
        {"render_items", Value(static_cast<double>(rt_get_render_items()))},
        {"gpu_count", Value(static_cast<double>(rt_backend_gpu_count()))}
    });
}

inline Value engine_render_stats(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.render_stats", args)) return *overrideResult;
    return engine_stats(args);
}

inline Value engine_camera_fov(const std::vector<Value>& args) {
    if (!args.empty()) {
        rt_set_camera_fov(static_cast<float>(to_number(args[0])));
    }
    return Value(static_cast<double>(rt_get_camera_fov()));
}

inline Value engine_camera_orbit(const std::vector<Value>& args) {
    const float yaw = args.size() > 0 ? static_cast<float>(to_number(args[0])) : 0.0f;
    const float pitch = args.size() > 1 ? static_cast<float>(to_number(args[1])) : 0.0f;
    const float radiusDelta = args.size() > 2 ? static_cast<float>(to_number(args[2])) : 0.0f;
    rt_camera_orbit(yaw, pitch, radiusDelta);
    return Value();
}

inline Value engine_key_down(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    return Value(rt_key_down(static_cast<int>(to_number(args[0]))) != 0);
}

inline Value engine_key_pressed(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    return Value(rt_key_pressed(static_cast<int>(to_number(args[0]))) != 0);
}

inline Value engine_mouse_down(const std::vector<Value>& args) {
    if (args.empty()) return Value(false);
    return Value(rt_mouse_down(static_cast<int>(to_number(args[0]))) != 0);
}

inline Value engine_mouse_pos(const std::vector<Value>& args) {
    (void)args;
    return array({Value(static_cast<double>(rt_mouse_x())), Value(static_cast<double>(rt_mouse_y()))});
}

inline Value engine_window(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.window", args)) return *overrideResult;
    return engine_init(args);
}

inline Value engine_camera(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.camera", args)) return *overrideResult;
    return engine_set_camera(args);
}

inline Value engine_frame_begin(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.frame_begin", args)) return *overrideResult;
    rt_begin();
    if (!args.empty()) {
        RTColor color = engine_color_from(args[0], RTColor{11, 18, 28, 255});
        rt_clear(color);
    }
    return Value();
}

inline Value engine_frame_end(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.frame_end", args)) return *overrideResult;
    (void)args;
    rt_end();
    return Value();
}

inline Value engine_scene(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene", args)) return *overrideResult;
    engine_current_scene_name() = args.empty() ? "main" : to_string(args[0]);
    engine_current_scene();
    return Value(engine_current_scene_name());
}

inline Value engine_scene_clear(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_clear", args)) return *overrideResult;
    (void)args;
    auto& scene = engine_current_scene();
    scene.entities.clear();
    scene.materials.clear();
    scene.meshes.clear();
    scene.textures.clear();
    scene.hasSun = false;
    return Value();
}

inline Value engine_scene_stats(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_stats", args)) return *overrideResult;
    return Value();
}

inline Value engine_export_scene(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.export_scene", args)) return *overrideResult;
    return Value();
}

inline Value engine_scene_save(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_save", args)) return *overrideResult;
    return Value();
}

inline Value engine_scene_load(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_load", args)) return *overrideResult;
    return Value();
}

inline Value engine_entity(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity", args)) return *overrideResult;
    if (args.size() < 5) return Value();

    EngineEntity entity;
    entity.kind = to_string(args[1]);
    entity.position = engine_vec3_from(args[2], entity.position);
    entity.color = engine_color_from(args[4], entity.color);

    if (entity.kind == "plane") {
        entity.planeSize = engine_vec2_from(args[3], entity.planeSize);
    } else if (entity.kind == "sphere") {
        if (is_array(args[3])) {
            auto items = args[3].as_array();
            if (!items->empty()) {
                entity.radius = static_cast<float>(to_number((*items)[0]));
            }
        } else {
            entity.radius = static_cast<float>(to_number(args[3]));
        }
    } else {
        entity.size = engine_vec3_from(args[3], entity.size);
    }

    if (args.size() > 5) {
        entity.material = to_string(args[5]);
    }
    if (args.size() > 6) {
        entity.mesh = to_string(args[6]);
    }
    if (args.size() > 7) {
        entity.texture = to_string(args[7]);
    }

    engine_current_scene().entities[to_string(args[0])] = entity;
    return Value();
}

inline Value engine_entity_remove(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_remove", args)) return *overrideResult;
    if (args.empty()) return Value(false);
    return Value(engine_current_scene().entities.erase(to_string(args[0])) > 0);
}

inline Value engine_entity_exists(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_exists", args)) return *overrideResult;
    if (args.empty()) return Value(false);
    return Value(engine_current_scene().entities.find(to_string(args[0])) != engine_current_scene().entities.end());
}

inline Value engine_entity_set_position(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_set_position", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.position = engine_vec3_from(args[1], found->second.position);
    return Value(true);
}

inline Value engine_entity_get_position(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_get_position", args)) return *overrideResult;
    if (args.empty()) return Value();
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value();
    return array({
        Value(static_cast<double>(found->second.position.x)),
        Value(static_cast<double>(found->second.position.y)),
        Value(static_cast<double>(found->second.position.z))
    });
}

inline Value engine_entity_set_size(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_set_size", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    if (found->second.kind == "plane") {
        found->second.planeSize = engine_vec2_from(args[1], found->second.planeSize);
    } else {
        found->second.size = engine_vec3_from(args[1], found->second.size);
    }
    return Value(true);
}

inline Value engine_entity_set_scale(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_set_scale", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.scale = engine_vec3_from(args[1], found->second.scale);
    return Value(true);
}

inline Value engine_entity_set_radius(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_set_radius", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    if (is_array(args[1])) {
        auto items = args[1].as_array();
        if (!items->empty()) {
            found->second.radius = static_cast<float>(to_number((*items)[0]));
        }
    } else {
        found->second.radius = static_cast<float>(to_number(args[1]));
    }
    return Value(true);
}

inline Value engine_entity_set_color(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_set_color", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.color = engine_color_from(args[1], found->second.color);
    return Value(true);
}

inline Value engine_entity_set_visible(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_set_visible", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.visible = truthy(args[1]);
    return Value(true);
}

inline Value engine_entity_mesh(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_mesh", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.mesh = to_string(args[1]);
    return Value(true);
}

inline Value engine_entity_texture(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_texture", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.texture = to_string(args[1]);
    return Value(true);
}

inline Value engine_mesh(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.mesh", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    scene.meshes[to_string(args[0])] = engine_mesh_from_args(args, 1);
    return Value(true);
}

inline Value engine_mesh_exists(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.mesh_exists", args)) return *overrideResult;
    if (args.empty()) return Value(false);
    auto& scene = engine_current_scene();
    return Value(scene.meshes.find(to_string(args[0])) != scene.meshes.end());
}

inline Value engine_texture(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.texture", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    scene.textures[to_string(args[0])] = engine_texture_from_args(args, 1);
    return Value(true);
}

inline Value engine_texture_exists(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.texture_exists", args)) return *overrideResult;
    if (args.empty()) return Value(false);
    auto& scene = engine_current_scene();
    return Value(scene.textures.find(to_string(args[0])) != scene.textures.end());
}

inline Value engine_material(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.material", args)) return *overrideResult;
    if (args.empty()) return Value();
    auto& scene = engine_current_scene();
    scene.materials[to_string(args[0])] = engine_material_from_args(args, 1);
    return Value(true);
}

inline Value engine_material_exists(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.material_exists", args)) return *overrideResult;
    if (args.empty()) return Value(false);
    auto& scene = engine_current_scene();
    return Value(scene.materials.find(to_string(args[0])) != scene.materials.end());
}

inline Value engine_material_texture(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.material_texture", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.materials.find(to_string(args[0]));
    if (found == scene.materials.end()) return Value(false);
    found->second.texture = to_string(args[1]);
    if (args.size() > 2) {
        found->second.normalTexture = to_string(args[2]);
    }
    return Value(true);
}

inline Value engine_entity_material(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.entity_material", args)) return *overrideResult;
    if (args.size() < 2) return Value(false);
    auto& scene = engine_current_scene();
    const auto found = scene.entities.find(to_string(args[0]));
    if (found == scene.entities.end()) return Value(false);
    found->second.material = to_string(args[1]);
    return Value(true);
}

inline Value engine_light_ambient(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.light_ambient", args)) return *overrideResult;
    if (args.empty()) return Value();
    engine_current_scene().ambient = engine_color_from(args[0], engine_current_scene().ambient);
    return Value();
}

inline Value engine_light_directional(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.light_directional", args)) return *overrideResult;
    if (args.size() < 3) return Value();
    auto& scene = engine_current_scene();
    scene.sun.direction = engine_vec3_from(args[0], scene.sun.direction);
    scene.sun.color = engine_color_from(args[1], scene.sun.color);
    scene.sun.intensity = static_cast<float>(to_number(args[2]));
    scene.hasSun = true;
    return Value();
}

inline Value engine_scene_draw(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.scene_draw", args)) return *overrideResult;
    auto& scene = engine_current_scene();
    const std::optional<EngineLight> light = scene.hasSun ? std::optional<EngineLight>(scene.sun) : std::nullopt;
    int submittedItems = 0;

    for (const auto& pair : scene.entities) {
        const EngineEntity& entity = pair.second;
        if (!entity.visible) {
            continue;
        }

        std::string drawKind = entity.kind;
        RTVec3 drawSize = entity.size;
        RTVec2 drawPlaneSize = entity.planeSize;
        float drawRadius = entity.radius;
        if (!entity.mesh.empty()) {
            const auto mesh = scene.meshes.find(entity.mesh);
            if (mesh != scene.meshes.end()) {
                if (!mesh->second.primitive.empty()) {
                    drawKind = mesh->second.primitive;
                }
                drawSize = mesh->second.defaultSize;
                drawPlaneSize = mesh->second.defaultPlaneSize;
                drawRadius = mesh->second.defaultRadius;
            }
        }

        RTColor baseColor = entity.color;
        const auto material = scene.materials.find(entity.material);
        if (material != scene.materials.end()) {
            baseColor = material->second.albedo;
            baseColor = engine_mix_color(baseColor, material->second.emissive, 0.2f);
        }
        submittedItems += 1;

        const RTColor shaded = engine_apply_light(baseColor, scene.ambient, light);
        if (drawKind == "plane") {
            rt_draw_plane(entity.position, RTVec2{drawPlaneSize.x * entity.scale.x, drawPlaneSize.y * entity.scale.z}, shaded);
        } else if (drawKind == "sphere") {
            const float radiusScale = std::max(entity.scale.x, std::max(entity.scale.y, entity.scale.z));
            rt_draw_sphere(entity.position, drawRadius * radiusScale, shaded);
        } else {
            rt_draw_cube(entity.position, engine_mul_vec3(drawSize, entity.scale), shaded);
        }
    }
    rt_set_render_items(submittedItems);
    return Value();
}

inline Value engine_assets_root(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.assets_root", args)) return *overrideResult;
    if (!args.empty()) {
        engine_assets_root_value() = to_string(args[0]);
    }
    return Value(engine_assets_root_value());
}

inline Value engine_asset_path(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.asset_path", args)) return *overrideResult;
    if (args.empty()) return Value(engine_assets_root_value());
    const std::filesystem::path path = std::filesystem::current_path() / engine_assets_root_value() / to_string(args[0]);
    return Value(path.lexically_normal().string());
}

inline Value engine_asset_exists(const std::vector<Value>& args) {
    if (const auto overrideResult = maybe_native_builtin_override("engine.asset_exists", args)) return *overrideResult;
    if (args.empty()) return Value(false);
    const std::filesystem::path path = std::filesystem::current_path() / engine_assets_root_value() / to_string(args[0]);
    return Value(std::filesystem::exists(path));
}

inline Value engine_draw_grid(const std::vector<Value>& args) {
    const int slices = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 20;
    const float spacing = args.size() > 1 ? static_cast<float>(to_number(args[1])) : 1.0f;
    rt_draw_grid(slices, spacing);
    return Value();
}

inline Value engine_draw_cube(const std::vector<Value>& args) {
    RTVec3 position{0.0f, 0.5f, 0.0f};
    RTVec3 size{1.0f, 1.0f, 1.0f};
    RTColor color{84, 165, 255, 255};

    if (args.size() > 0) position = engine_vec3_from(args[0], position);
    if (args.size() > 1) size = engine_vec3_from(args[1], size);
    if (args.size() > 2) color = engine_color_from(args[2], color);

    rt_draw_cube(position, size, color);
    return Value();
}

inline Value engine_draw_plane(const std::vector<Value>& args) {
    RTVec3 position{0.0f, 0.0f, 0.0f};
    RTVec2 size{10.0f, 10.0f};
    RTColor color{32, 54, 82, 255};

    if (args.size() > 0) position = engine_vec3_from(args[0], position);
    if (args.size() > 1 && is_array(args[1])) {
        auto items = args[1].as_array();
        if (items->size() >= 2) {
            size = RTVec2{
                static_cast<float>(to_number((*items)[0])),
                static_cast<float>(to_number((*items)[1]))
            };
        }
    }
    if (args.size() > 2) color = engine_color_from(args[2], color);

    rt_draw_plane(position, size, color);
    return Value();
}

inline Value engine_draw_sphere(const std::vector<Value>& args) {
    RTVec3 position{0.0f, 1.0f, 0.0f};
    float radius = 1.0f;
    RTColor color{255, 146, 76, 255};

    if (args.size() > 0) position = engine_vec3_from(args[0], position);
    if (args.size() > 1) radius = static_cast<float>(to_number(args[1]));
    if (args.size() > 2) color = engine_color_from(args[2], color);

    rt_draw_sphere(position, radius, color);
    return Value();
}

inline Value engine_draw_text(const std::vector<Value>& args) {
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const int x = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 20;
    const int y = args.size() > 2 ? static_cast<int>(to_number(args[2])) : 20;
    const int fontSize = args.size() > 3 ? static_cast<int>(to_number(args[3])) : 20;
    RTColor color{231, 236, 245, 255};
    if (args.size() > 4) color = engine_color_from(args[4], color);
    rt_draw_text(text.c_str(), x, y, fontSize, color);
    return Value();
}

inline Value engine_draw_fps(const std::vector<Value>& args) {
    const int x = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 16;
    const int y = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 16;
    rt_draw_fps(x, y);
    return Value();
}
}

)";
    }
};
