#include "BuiltinModules.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "Framework.h"
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

char* duplicate_string(const std::string& value) {
    char* memory = new char[value.size() + 1];
    std::memcpy(memory, value.c_str(), value.size() + 1);
    return memory;
}

std::string escape_json(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '"') result += "\\\"";
        else if (ch == '\\') result += "\\\\";
        else if (ch == '\n') result += "\\n";
        else if (ch == '\r') result += "\\r";
        else if (ch == '\t') result += "\\t";
        else result += ch;
    }
    return result;
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

#ifdef _WIN32
RayQuiroApp& runtime() {
    static RayQuiroApp app;
    return app;
}

struct NativeAppWindow {
    HWND hwnd = nullptr;
    bool open = false;
    std::string title;
    int width = 0;
    int height = 0;
};

static std::unordered_map<int, NativeAppWindow> g_appWindows;
static int g_nextAppWindowId = 1;
static bool g_classRegistered = false;

static LRESULT CALLBACK RayQuiroWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CLOSE) {
        for (auto& pair : g_appWindows) {
            if (pair.second.hwnd == hwnd) {
                pair.second.open = false;
            }
        }
        ShowWindow(hwnd, SW_HIDE);
        DestroyWindow(hwnd);
        return 0;
    }
    if (uMsg == WM_DESTROY) {
        for (auto& pair : g_appWindows) {
            if (pair.second.hwnd == hwnd) {
                pair.second.open = false;
            }
        }
        return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void ensure_window_class() {
    if (g_classRegistered) return;
    WNDCLASSA wc = {};
    wc.lpfnWndProc = RayQuiroWindowProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "RayQuiroNativeWindowClass";
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);
    g_classRegistered = true;
}

static int extract_window_id(const std::vector<AppJsonValue>& args) {
    if (args.empty()) return 0;
    if (args[0].type == AppJsonValue::Number) return args[0].to_int(0);
    if (args[0].type == AppJsonValue::Object) {
        auto it = args[0].obj.find("id");
        if (it != args[0].obj.end()) return it->second.to_int(0);
    }
    return 0;
}
#endif

std::string dispatch(const std::string& functionName, const std::vector<AppJsonValue>& args) {
#ifdef _WIN32
    if (functionName == "window") {
        std::string title = "RayQuiro";
        int width = 800;
        int height = 600;
        bool resizable = true;
        if (!args.empty()) {
            if (args[0].type == AppJsonValue::Object) {
                auto itTitle = args[0].obj.find("title");
                if (itTitle != args[0].obj.end()) title = itTitle->second.to_string();
                auto itWidth = args[0].obj.find("width");
                if (itWidth != args[0].obj.end()) width = itWidth->second.to_int(800);
                auto itHeight = args[0].obj.find("height");
                if (itHeight != args[0].obj.end()) height = itHeight->second.to_int(600);
                auto itRes = args[0].obj.find("resizable");
                if (itRes != args[0].obj.end()) resizable = itRes->second.to_bool(true);
            } else if (args[0].type == AppJsonValue::String) {
                title = args[0].to_string();
                if (args.size() > 1) width = args[1].to_int(800);
                if (args.size() > 2) height = args[2].to_int(600);
            }
        }
        ensure_window_class();
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (resizable) style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        RECT rect = {0, 0, width, height};
        AdjustWindowRect(&rect, style, FALSE);
        HWND hwnd = CreateWindowExA(0, "RayQuiroNativeWindowClass", title.c_str(),
            style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left, rect.bottom - rect.top,
            NULL, NULL, GetModuleHandleA(NULL), NULL);
        if (!hwnd) throw std::runtime_error("Failed to create native window.");
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        int id = g_nextAppWindowId++;
        NativeAppWindow win;
        win.hwnd = hwnd;
        win.open = true;
        win.title = title;
        win.width = width;
        win.height = height;
        g_appWindows[id] = win;
        return "{\"__type__\":\"Window\",\"id\":" + std::to_string(id) + ",\"title\":\"" + escape_json(title) + "\",\"width\":" + std::to_string(width) + ",\"height\":" + std::to_string(height) + ",\"open\":true}";
    }
    if (functionName == "window_is_open" || functionName == "isOpen") {
        int id = extract_window_id(args);
        auto it = g_appWindows.find(id);
        if (it == g_appWindows.end()) return "false";
        if (!it->second.open || !IsWindow(it->second.hwnd)) {
            it->second.open = false;
            return "false";
        }
        return "true";
    }
    if (functionName == "window_poll_events" || functionName == "pollEvents") {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                for (auto& pair : g_appWindows) pair.second.open = false;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        return "null";
    }
    if (functionName == "window_set_title" || functionName == "setTitle") {
        int id = extract_window_id(args);
        std::string newTitle = args.size() > 1 ? args[1].to_string() : (args.size() > 0 ? args[0].to_string() : "");
        auto it = g_appWindows.find(id);
        if (it != g_appWindows.end() && IsWindow(it->second.hwnd)) {
            it->second.title = newTitle;
            SetWindowTextA(it->second.hwnd, newTitle.c_str());
        }
        return "null";
    }
    if (functionName == "window_set_size" || functionName == "setSize") {
        int id = extract_window_id(args);
        int w = args.size() > 1 ? args[1].to_int(800) : 800;
        int h = args.size() > 2 ? args[2].to_int(600) : 600;
        auto it = g_appWindows.find(id);
        if (it != g_appWindows.end() && IsWindow(it->second.hwnd)) {
            it->second.width = w;
            it->second.height = h;
            SetWindowPos(it->second.hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
        }
        return "null";
    }
    if (functionName == "window_close" || functionName == "close") {
        int id = extract_window_id(args);
        auto it = g_appWindows.find(id);
        if (it != g_appWindows.end()) {
            it->second.open = false;
            if (IsWindow(it->second.hwnd)) DestroyWindow(it->second.hwnd);
        }
        return "null";
    }
    if (functionName == "quit") {
        for (auto& pair : g_appWindows) {
            pair.second.open = false;
            if (IsWindow(pair.second.hwnd)) DestroyWindow(pair.second.hwnd);
        }
        PostQuitMessage(0);
        return "null";
    }
    if (functionName == "init") {
        runtime().init(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_int(900) : 900, args.size() > 2 ? args[2].to_int(600) : 600);
        return "null";
    }
    if (functionName == "run") { runtime().run(); return "null"; }
    if (functionName == "button") {
        if (args.size() < 5) return "null";
        runtime().add_button(args[0].to_string(), args[1].to_int(), args[2].to_int(), args[3].to_int(), args[4].to_int());
        return "null";
    }
    if (functionName == "text") {
        if (args.size() < 5) return "null";
        runtime().add_text(args[0].to_string(), args[1].to_int(), args[2].to_int(), args[3].to_int(), args[4].to_int());
        return "null";
    }
    if (functionName == "msg") {
        RayQuiroApp::show_message(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_string() : "");
        return "null";
    }
#else
    if (functionName == "window") {
        return "{\"__type__\":\"Window\",\"id\":1,\"title\":\"RayQuiro\",\"width\":800,\"height\":600,\"open\":true}";
    }
    if (functionName == "window_is_open" || functionName == "isOpen") return "false";
    if (functionName == "window_poll_events" || functionName == "pollEvents") return "null";
    if (functionName == "window_set_title" || functionName == "setTitle") return "null";
    if (functionName == "window_set_size" || functionName == "setSize") return "null";
    if (functionName == "window_close" || functionName == "close") return "null";
    if (functionName == "quit") return "null";
#endif
    throw std::runtime_error("rayquiro.app does not export app." + functionName);
}

}

int rqm_builtin_app_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message) {
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
