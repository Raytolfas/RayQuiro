#include "BuiltinModules.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "Framework.h"

#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <fstream>

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

std::string serialize_json_val(const AppJsonValue& val) {
    if (val.type == AppJsonValue::String) return "\"" + escape_json(val.str) + "\"";
    if (val.type == AppJsonValue::Number) return val.to_string();
    if (val.type == AppJsonValue::Bool) return val.b ? "true" : "false";
    if (val.type == AppJsonValue::Null) return "null";
    if (val.type == AppJsonValue::Array) {
        std::string s = "[";
        for (size_t i = 0; i < val.arr.size(); ++i) {
            if (i > 0) s += ",";
            s += serialize_json_val(val.arr[i]);
        }
        s += "]";
        return s;
    }
    if (val.type == AppJsonValue::Object) {
        std::string s = "{";
        bool first = true;
        for (const auto& pair : val.obj) {
            if (!first) s += ",";
            first = false;
            s += "\"" + escape_json(pair.first) + "\":" + serialize_json_val(pair.second);
        }
        s += "}";
        return s;
    }
    return "null";
}

static int g_elementIdCounter = 1;

static std::string make_element(
    const std::string& tag,
    const std::string& text,
    const AppJsonValue& props,
    const std::vector<AppJsonValue>& children,
    const std::string& defaultStyleJson
) {
    std::string id = tag + "_" + std::to_string(g_elementIdCounter++);
    if (props.type == AppJsonValue::Object) {
        auto it = props.obj.find("id");
        if (it != props.obj.end() && !it->second.to_string().empty()) {
            id = it->second.to_string();
        }
    }

    std::string out = "{\"__type__\":\"Element\",\"tag\":\"" + escape_json(tag) + "\",\"id\":\"" + escape_json(id) + "\",\"text\":\"" + escape_json(text) + "\"";

    out += ",\"props\":{";
    bool firstP = true;
    if (props.type == AppJsonValue::Object) {
        for (const auto& pair : props.obj) {
            if (pair.first == "id" || pair.first == "style") continue;
            if (!firstP) out += ",";
            firstP = false;
            out += "\"" + escape_json(pair.first) + "\":" + serialize_json_val(pair.second);
        }
    }
    out += "}";

    out += ",\"style\":";
    if (props.type == AppJsonValue::Object && props.obj.find("style") != props.obj.end()) {
        out += serialize_json_val(props.obj.at("style"));
    } else {
        out += defaultStyleJson;
    }

    out += ",\"children\":[";
    for (size_t i = 0; i < children.size(); ++i) {
        if (i > 0) out += ",";
        out += serialize_json_val(children[i]);
    }
    out += "]}";

    return out;
}

static std::string style_to_inline_css(const AppJsonValue& style) {
    if (style.type != AppJsonValue::Object) return "";
    std::string css;
    for (const auto& pair : style.obj) {
        css += pair.first + ":" + pair.second.to_string() + ";";
    }
    return css;
}

static std::string render_html(const AppJsonValue& el) {
    if (el.type != AppJsonValue::Object) return "";
    std::string tag = "div";
    auto itTag = el.obj.find("tag");
    if (itTag != el.obj.end()) tag = itTag->second.to_string();

    std::string text;
    auto itText = el.obj.find("text");
    if (itText != el.obj.end()) text = itText->second.to_string();

    std::string id;
    auto itId = el.obj.find("id");
    if (itId != el.obj.end()) id = itId->second.to_string();

    std::string styleAttr;
    auto itStyle = el.obj.find("style");
    if (itStyle != el.obj.end()) {
        std::string inlineCss = style_to_inline_css(itStyle->second);
        if (!inlineCss.empty()) styleAttr = " style=\"" + inlineCss + "\"";
    }

    std::string propsAttr;
    auto itProps = el.obj.find("props");
    if (itProps != el.obj.end() && itProps->second.type == AppJsonValue::Object) {
        for (const auto& pair : itProps->second.obj) {
            propsAttr += " " + pair.first + "=\"" + pair.second.to_string() + "\"";
        }
    }

    if (tag == "hr") return "<hr" + styleAttr + " />";
    if (tag == "input") return "<input" + propsAttr + styleAttr + " />";
    if (tag == "textarea") return "<textarea" + propsAttr + styleAttr + ">" + text + "</textarea>";
    if (tag == "image") {
        std::string src;
        if (itProps != el.obj.end() && itProps->second.type == AppJsonValue::Object) {
            auto itSrc = itProps->second.obj.find("src");
            if (itSrc != itProps->second.obj.end()) src = itSrc->second.to_string();
        }
        return "<img src=\"" + src + "\"" + styleAttr + " />";
    }

    std::string inner = text;
    auto itChildren = el.obj.find("children");
    if (itChildren != el.obj.end() && itChildren->second.type == AppJsonValue::Array) {
        for (const auto& child : itChildren->second.arr) {
            inner += render_html(child);
        }
    }

    std::string htmlTag = tag;
    if (tag == "text" || tag == "label") htmlTag = "span";
    else if (tag == "row" || tag == "column" || tag == "box" || tag == "card") htmlTag = "div";

    return "<" + htmlTag + (!id.empty() ? (" id=\"" + id + "\"") : "") + propsAttr + styleAttr + ">" + inner + "</" + htmlTag + ">";
}

static std::string escape_slint_str(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') continue;
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string render_slint_node(const AppJsonValue& el, int indent = 12) {
    if (el.type != AppJsonValue::Object) return "";
    std::string tag = "column";
    auto itTag = el.obj.find("tag");
    if (itTag != el.obj.end()) tag = itTag->second.to_string();

    std::string text;
    auto itText = el.obj.find("text");
    if (itText != el.obj.end()) text = itText->second.to_string();

    std::vector<AppJsonValue> children;
    auto itChildren = el.obj.find("children");
    if (itChildren != el.obj.end() && itChildren->second.type == AppJsonValue::Array) {
        children = itChildren->second.arr;
    }

    std::string pad(indent, ' ');
    std::string innerPad(indent + 4, ' ');

    if (tag == "button") {
        return pad + "AppButton {\n" + innerPad + "text: \"" + escape_slint_str(text.empty() ? "Button" : text) + "\";\n" + pad + "}\n";
    }

    if (tag == "text" || tag == "label") {
        return pad + "Text {\n" + innerPad + "text: \"" + escape_slint_str(text) + "\";\n" + innerPad + "color: #a1a1aa;\n" + innerPad + "font-size: 13px;\n" + pad + "}\n";
    }

    if (tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6" || tag == "heading") {
        std::string fsize = "16px";
        if (tag == "h1") fsize = "22px";
        else if (tag == "h2") fsize = "18px";
        else if (tag == "h3") fsize = "15px";
        return pad + "Text {\n" + innerPad + "text: \"" + escape_slint_str(text) + "\";\n" + innerPad + "color: #f4f4f5;\n" + innerPad + "font-size: " + fsize + ";\n" + innerPad + "font-weight: 700;\n" + pad + "}\n";
    }

    if (tag == "input" || tag == "text_input" || tag == "textInput") {
        std::string placeholder;
        auto itProps = el.obj.find("props");
        if (itProps != el.obj.end() && itProps->second.type == AppJsonValue::Object) {
            auto itPh = itProps->second.obj.find("placeholder");
            if (itPh != itProps->second.obj.end()) placeholder = itPh->second.to_string();
        }
        return pad + "LineEdit {\n" + innerPad + "placeholder-text: \"" + escape_slint_str(placeholder) + "\";\n" + innerPad + "text: \"" + escape_slint_str(text) + "\";\n" + innerPad + "height: 34px;\n" + pad + "}\n";
    }

    if (tag == "textarea") {
        return pad + "TextEdit {\n" + innerPad + "text: \"" + escape_slint_str(text) + "\";\n" + innerPad + "height: 80px;\n" + pad + "}\n";
    }

    if (tag == "checkbox") {
        bool checked = false;
        auto itProps = el.obj.find("props");
        if (itProps != el.obj.end() && itProps->second.type == AppJsonValue::Object) {
            auto itChk = itProps->second.obj.find("checked");
            if (itChk != itProps->second.obj.end()) checked = itChk->second.to_bool(false);
        }
        return pad + "CheckBox {\n" + innerPad + "text: \"" + escape_slint_str(text) + "\";\n" + innerPad + "checked: " + (checked ? "true" : "false") + ";\n" + pad + "}\n";
    }

    if (tag == "slider") {
        return pad + "Slider {\n" + innerPad + "minimum: 0;\n" + innerPad + "maximum: 100;\n" + innerPad + "value: 50;\n" + pad + "}\n";
    }

    if (tag == "progress") {
        return pad + "ProgressIndicator {\n" + innerPad + "progress: 0.5;\n" + pad + "}\n";
    }

    if (tag == "hr" || tag == "divider" || tag == "separator") {
        return pad + "Rectangle {\n" + innerPad + "height: 1px;\n" + innerPad + "background: #3f3f46;\n" + pad + "}\n";
    }

    if (tag == "row") {
        std::string res = pad + "HorizontalLayout {\n" + innerPad + "spacing: 12px;\n" + innerPad + "alignment: start;\n";
        for (const auto& child : children) {
            res += render_slint_node(child, indent + 4);
        }
        res += pad + "}\n";
        return res;
    }

    if (tag == "column") {
        std::string res = pad + "VerticalLayout {\n" + innerPad + "spacing: 12px;\n" + innerPad + "alignment: start;\n";
        for (const auto& child : children) {
            res += render_slint_node(child, indent + 4);
        }
        res += pad + "}\n";
        return res;
    }

    if (tag == "card" || tag == "box") {
        std::string res = pad + "Rectangle {\n" + innerPad + "background: #27272a;\n" + innerPad + "border-radius: 8px;\n" + innerPad + "border-width: 1px;\n" + innerPad + "border-color: #3f3f46;\n";
        res += innerPad + "VerticalLayout {\n" + innerPad + "    padding: 16px;\n" + innerPad + "    spacing: 10px;\n";
        for (const auto& child : children) {
            res += render_slint_node(child, indent + 8);
        }
        res += innerPad + "}\n" + pad + "}\n";
        return res;
    }

    std::string res = pad + "VerticalLayout {\n" + innerPad + "spacing: 8px;\n";
    for (const auto& child : children) {
        res += render_slint_node(child, indent + 4);
    }
    res += pad + "}\n";
    return res;
}

static std::string compile_element_to_slint_app(const AppJsonValue& root, const std::string& title, int width, int height) {
    std::string out = R"(import { Button, Slider, LineEdit, TextEdit, CheckBox, ProgressIndicator, ScrollView } from "std-widgets.slint";

component AppButton inherits Rectangle {
    in property <string> text: "Action";
    in property <bool> primary: false;
    callback clicked();

    height: 34px;
    border-radius: 6px;
    background: root.primary ? (touch.has-hover ? #2563eb : #3b82f6) : (touch.has-hover ? #323238 : #27272a);
    border-width: 1px;
    border-color: root.primary ? #3b82f6 : #3f3f46;
    animate background { duration: 100ms; }

    touch := TouchArea {
        clicked => { root.clicked(); }
    }

    HorizontalLayout {
        padding-left: 14px;
        padding-right: 14px;
        alignment: center;
        Text {
            text: root.text;
            color: #f4f4f5;
            font-size: 13px;
            font-weight: 500;
            vertical-alignment: center;
        }
    }
}

export component MainWindow inherits Window {
    title: ")";
    out += escape_slint_str(title);
    out += R"(";
    background: #18181b;
    preferred-width: )";
    out += std::to_string(width) + "px;\n";
    out += "    preferred-height: " + std::to_string(height) + "px;\n";
    out += R"(    min-width: 640px;
    min-height: 480px;

    ScrollView {
        VerticalLayout {
            padding: 20px;
            spacing: 16px;
            alignment: start;
)";
    out += render_slint_node(root, 12);
    out += R"(        }
    }
}
)";
    return out;
}

RayQuiroApp& runtime() {
    return RayQuiroApp::instance();
}

#ifdef _WIN32

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
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
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

static bool parse_hex_color(const std::string& raw, int& r, int& g, int& b) {
    std::string hex = raw;
    while (!hex.empty() && (hex.front() == ' ' || hex.front() == '\t')) hex.erase(hex.begin());
    while (!hex.empty() && (hex.back() == ' ' || hex.back() == '\t')) hex.pop_back();
    if (hex.empty()) return false;
    if (hex[0] == '#') hex = hex.substr(1);
    if (hex.size() == 3) {
        try {
            int cr = std::stoi(hex.substr(0, 1), nullptr, 16);
            int cg = std::stoi(hex.substr(1, 1), nullptr, 16);
            int cb = std::stoi(hex.substr(2, 1), nullptr, 16);
            r = cr * 17;
            g = cg * 17;
            b = cb * 17;
            return true;
        } catch (...) {
            return false;
        }
    }
    if (hex.size() == 6 || hex.size() == 8) {
        try {
            r = std::stoi(hex.substr(0, 2), nullptr, 16);
            g = std::stoi(hex.substr(2, 2), nullptr, 16);
            b = std::stoi(hex.substr(4, 2), nullptr, 16);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

static std::string make_hex(int r, int g, int b) {
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
}

static std::string blend_colors(const std::string& c1, const std::string& c2, double t) {
    int r1 = 0, g1 = 0, b1 = 0;
    int r2 = 0, g2 = 0, b2 = 0;
    if (!parse_hex_color(c1, r1, g1, b1)) { r1 = 0; g1 = 0; b1 = 0; }
    if (!parse_hex_color(c2, r2, g2, b2)) { r2 = 255; g2 = 255; b2 = 255; }
    t = std::max(0.0, std::min(1.0, t));
    int r = static_cast<int>(r1 + (r2 - r1) * t);
    int g = static_cast<int>(g1 + (g2 - g1) * t);
    int b = static_cast<int>(b1 + (b2 - b1) * t);
    return make_hex(r, g, b);
}

static std::string lighten_color(const std::string& c, double factor) {
    int r = 0, g = 0, b = 0;
    if (!parse_hex_color(c, r, g, b)) return "#ffffff";
    factor = std::max(0.0, std::min(1.0, factor));
    r = static_cast<int>(r + (255 - r) * factor);
    g = static_cast<int>(g + (255 - g) * factor);
    b = static_cast<int>(b + (255 - b) * factor);
    return make_hex(r, g, b);
}

static std::string darken_color(const std::string& c, double factor) {
    int r = 0, g = 0, b = 0;
    if (!parse_hex_color(c, r, g, b)) return "#000000";
    factor = std::max(0.0, std::min(1.0, factor));
    r = static_cast<int>(r * (1.0 - factor));
    g = static_cast<int>(g * (1.0 - factor));
    b = static_cast<int>(b * (1.0 - factor));
    return make_hex(r, g, b);
}

static std::string get_app_theme_json(const std::string& name) {
    if (name == "light") {
        return "{\"name\":\"light\",\"background\":\"#ffffff\",\"surface\":\"#f4f4f5\",\"border\":\"#e4e4e7\",\"text\":\"#09090b\",\"textMuted\":\"#71717a\",\"primary\":\"#2563eb\",\"primaryHover\":\"#1d4ed8\",\"secondary\":\"#64748b\",\"success\":\"#16a34a\",\"warning\":\"#ca8a04\",\"danger\":\"#dc2626\",\"radius\":8,\"font\":\"Segoe UI\"}";
    }
    if (name == "nord") {
        return "{\"name\":\"nord\",\"background\":\"#2e3440\",\"surface\":\"#3b4252\",\"border\":\"#4c566a\",\"text\":\"#eceff4\",\"textMuted\":\"#d8dee9\",\"primary\":\"#88c0d0\",\"primaryHover\":\"#81a1c1\",\"secondary\":\"#4c566a\",\"success\":\"#a3be8c\",\"warning\":\"#ebcb8b\",\"danger\":\"#bf616a\",\"radius\":6,\"font\":\"Segoe UI\"}";
    }
    return "{\"name\":\"dark\",\"background\":\"#18181b\",\"surface\":\"#27272a\",\"border\":\"#3f3f46\",\"text\":\"#f4f4f5\",\"textMuted\":\"#a1a1aa\",\"primary\":\"#3b82f6\",\"primaryHover\":\"#2563eb\",\"secondary\":\"#64748b\",\"success\":\"#22c55e\",\"warning\":\"#eab308\",\"danger\":\"#ef4444\",\"radius\":8,\"font\":\"Segoe UI\"}";
}

static const char* g_default_slint_app = R"(import { Button } from "std-widgets.slint";

export component MainWindow inherits Window {
    title: "RayQuiro Application";
    width: 800px;
    height: 600px;
    background: #0f172a;

    VerticalLayout {
        alignment: center;
        spacing: 16px;

        Text {
            text: "RayQuiro Application";
            color: #f8fafc;
            font-size: 24px;
            font-weight: 700;
            horizontal-alignment: center;
        }

        Text {
            text: "Build native UI declaratively using rayquiro.app widgets in pure .rq";
            color: #94a3b8;
            font-size: 14px;
            horizontal-alignment: center;
        }
    }
}
)";

std::string dispatch(const std::string& functionName, const std::vector<AppJsonValue>& args) {
    if (functionName == "stylesheet") {
        if (!args.empty()) runtime().set_style(args[0].to_string());
        return "null";
    }
    if (functionName == "dark") return get_app_theme_json("dark");
    if (functionName == "light") return get_app_theme_json("light");
    if (functionName == "nord") return get_app_theme_json("nord");
    if (functionName == "rgb") {
        int r = args.size() > 0 ? args[0].to_int() : 0;
        int g = args.size() > 1 ? args[1].to_int() : 0;
        int b = args.size() > 2 ? args[2].to_int() : 0;
        return "\"" + make_hex(r, g, b) + "\"";
    }
    if (functionName == "rgba") {
        int r = args.size() > 0 ? args[0].to_int() : 0;
        int g = args.size() > 1 ? args[1].to_int() : 0;
        int b = args.size() > 2 ? args[2].to_int() : 0;
        double a = args.size() > 3 ? args[3].to_number(1.0) : 1.0;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "\"rgba(%d, %d, %d, %.3g)\"", r, g, b, a);
        return std::string(buf);
    }
    if (functionName == "hex") {
        int r = 0, g = 0, b = 0;
        if (!args.empty() && parse_hex_color(args[0].to_string(), r, g, b)) {
            return "\"" + make_hex(r, g, b) + "\"";
        }
        return "\"#000000\"";
    }
    if (functionName == "blend") {
        std::string c1 = args.size() > 0 ? args[0].to_string() : "#000000";
        std::string c2 = args.size() > 1 ? args[1].to_string() : "#ffffff";
        double t = args.size() > 2 ? args[2].to_number(0.5) : 0.5;
        return "\"" + blend_colors(c1, c2, t) + "\"";
    }
    if (functionName == "lighten") {
        std::string c = args.size() > 0 ? args[0].to_string() : "#000000";
        double f = args.size() > 1 ? args[1].to_number(0.1) : 0.1;
        return "\"" + lighten_color(c, f) + "\"";
    }
    if (functionName == "darken") {
        std::string c = args.size() > 0 ? args[0].to_string() : "#ffffff";
        double f = args.size() > 1 ? args[1].to_number(0.1) : 0.1;
        return "\"" + darken_color(c, f) + "\"";
    }
    if (functionName == "alpha") {
        std::string c = args.size() > 0 ? args[0].to_string() : "#000000";
        double a = args.size() > 1 ? args[1].to_number(1.0) : 1.0;
        int r = 0, g = 0, b = 0;
        parse_hex_color(c, r, g, b);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "\"rgba(%d, %d, %d, %.3g)\"", r, g, b, a);
        return std::string(buf);
    }
    if (functionName == "button") {
        std::string text = args.size() > 0 ? args[0].to_string() : "Button";
        int bx = args.size() > 1 && args[1].type == AppJsonValue::Number ? args[1].to_int(-1) : -1;
        int by = args.size() > 2 && args[2].type == AppJsonValue::Number ? args[2].to_int(-1) : -1;
        int bw = args.size() > 3 && args[3].type == AppJsonValue::Number ? args[3].to_int(-1) : -1;
        int bh = args.size() > 4 && args[4].type == AppJsonValue::Number ? args[4].to_int(-1) : -1;
        runtime().add_button(text, bx, by, bw, bh);
        AppJsonValue opts = args.size() > 1 ? args[1] : AppJsonValue{};
        return make_element("button", text, opts, {}, "{\"background\":\"#f4f4f5\",\"color\":\"#18181b\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"6px 12px\",\"cursor\":\"pointer\"}");
    }
    if (functionName == "text" || functionName == "label") {
        std::string text = args.size() > 0 ? args[0].to_string() : "";
        int tx = args.size() > 1 && args[1].type == AppJsonValue::Number ? args[1].to_int(-1) : -1;
        int ty = args.size() > 2 && args[2].type == AppJsonValue::Number ? args[2].to_int(-1) : -1;
        int tw = args.size() > 3 && args[3].type == AppJsonValue::Number ? args[3].to_int(-1) : -1;
        int th = args.size() > 4 && args[4].type == AppJsonValue::Number ? args[4].to_int(-1) : -1;
        runtime().add_text(text, tx, ty, tw, th);
        AppJsonValue opts = args.size() > 1 ? args[1] : AppJsonValue{};
        return make_element(functionName, text, opts, {}, "{\"color\":\"#18181b\",\"fontSize\":14}");
    }
    if (functionName == "input" || functionName == "text_input" || functionName == "textInput") {
        std::string placeholder = args.size() > 0 ? args[0].to_string() : "";
        std::string val = args.size() > 1 ? args[1].to_string() : "";
        int ix = args.size() > 1 && args[1].type == AppJsonValue::Number ? args[1].to_int(-1) : -1;
        int iy = args.size() > 2 && args[2].type == AppJsonValue::Number ? args[2].to_int(-1) : -1;
        int iw = args.size() > 3 && args[3].type == AppJsonValue::Number ? args[3].to_int(-1) : -1;
        int ih = args.size() > 4 && args[4].type == AppJsonValue::Number ? args[4].to_int(-1) : -1;
        runtime().add_input(placeholder, ix, iy, iw, ih);
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["placeholder"] = AppJsonValue{AppJsonValue::String, false, 0.0, placeholder, {}, {}};
        opts.obj["value"] = AppJsonValue{AppJsonValue::String, false, 0.0, val, {}, {}};
        return make_element("input", "", opts, {}, "{\"background\":\"#ffffff\",\"color\":\"#18181b\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"6px 8px\"}");
    }
    if (functionName == "textarea") {
        std::string placeholder = args.size() > 0 ? args[0].to_string() : "";
        std::string val = args.size() > 1 ? args[1].to_string() : "";
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["placeholder"] = AppJsonValue{AppJsonValue::String, false, 0.0, placeholder, {}, {}};
        return make_element("textarea", val, opts, {}, "{\"background\":\"#ffffff\",\"color\":\"#18181b\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"8px\"}");
    }
    if (functionName == "checkbox") {
        std::string label = args.size() > 0 ? args[0].to_string() : "";
        bool checked = args.size() > 1 ? args[1].to_bool(false) : false;
        int cx = args.size() > 2 && args[2].type == AppJsonValue::Number ? args[2].to_int(-1) : -1;
        int cy = args.size() > 3 && args[3].type == AppJsonValue::Number ? args[3].to_int(-1) : -1;
        int cw = args.size() > 4 && args[4].type == AppJsonValue::Number ? args[4].to_int(-1) : -1;
        int ch = args.size() > 5 && args[5].type == AppJsonValue::Number ? args[5].to_int(-1) : -1;
        runtime().add_checkbox(label, checked, cx, cy, cw, ch);
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["label"] = AppJsonValue{AppJsonValue::String, false, 0.0, label, {}, {}};
        opts.obj["checked"] = AppJsonValue{AppJsonValue::Bool, checked, 0.0, "", {}, {}};
        return make_element("checkbox", label, opts, {}, "{\"display\":\"inline-flex\",\"alignItems\":\"center\",\"gap\":\"6px\"}");
    }
    if (functionName == "radio") {
        std::string label = args.size() > 0 ? args[0].to_string() : "";
        std::string group = args.size() > 1 ? args[1].to_string() : "";
        bool checked = args.size() > 2 ? args[2].to_bool(false) : false;
        AppJsonValue opts = args.size() > 3 ? args[3] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["label"] = AppJsonValue{AppJsonValue::String, false, 0.0, label, {}, {}};
        opts.obj["group"] = AppJsonValue{AppJsonValue::String, false, 0.0, group, {}, {}};
        opts.obj["checked"] = AppJsonValue{AppJsonValue::Bool, checked, 0.0, "", {}, {}};
        return make_element("radio", label, opts, {}, "{\"display\":\"inline-flex\",\"alignItems\":\"center\",\"gap\":\"6px\"}");
    }
    if (functionName == "slider") {
        double minVal = args.size() > 0 ? args[0].to_number(0.0) : 0.0;
        double maxVal = args.size() > 1 ? args[1].to_number(100.0) : 100.0;
        double curVal = args.size() > 2 ? args[2].to_number(50.0) : 50.0;
        AppJsonValue opts = args.size() > 3 ? args[3] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["min"] = AppJsonValue{AppJsonValue::Number, false, minVal, "", {}, {}};
        opts.obj["max"] = AppJsonValue{AppJsonValue::Number, false, maxVal, "", {}, {}};
        opts.obj["value"] = AppJsonValue{AppJsonValue::Number, false, curVal, "", {}, {}};
        return make_element("slider", "", opts, {}, "{\"width\":\"100%\",\"accentColor\":\"#3b82f6\"}");
    }
    if (functionName == "progress") {
        double curVal = args.size() > 0 ? args[0].to_number(0.0) : 0.0;
        double maxVal = args.size() > 1 ? args[1].to_number(100.0) : 100.0;
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["value"] = AppJsonValue{AppJsonValue::Number, false, curVal, "", {}, {}};
        opts.obj["max"] = AppJsonValue{AppJsonValue::Number, false, maxVal, "", {}, {}};
        return make_element("progress", "", opts, {}, "{\"width\":\"100%\",\"height\":\"12px\",\"radius\":6}");
    }
    if (functionName == "select") {
        AppJsonValue optionsList = args.size() > 0 ? args[0] : AppJsonValue{};
        int selIdx = args.size() > 1 ? args[1].to_int(0) : 0;
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["options"] = optionsList;
        opts.obj["selectedIndex"] = AppJsonValue{AppJsonValue::Number, false, static_cast<double>(selIdx), "", {}, {}};
        return make_element("select", "", opts, {}, "{\"background\":\"#ffffff\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"6px 10px\"}");
    }
    if (functionName == "image") {
        std::string src = args.size() > 0 ? args[0].to_string() : "";
        AppJsonValue opts = args.size() > 1 ? args[1] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["src"] = AppJsonValue{AppJsonValue::String, false, 0.0, src, {}, {}};
        return make_element("image", "", opts, {}, "{\"maxWidth\":\"100%\",\"display\":\"block\"}");
    }
    if (functionName == "link") {
        std::string href = args.size() > 0 ? args[0].to_string() : "#";
        std::string text = args.size() > 1 ? args[1].to_string() : href;
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        opts.type = AppJsonValue::Object;
        opts.obj["href"] = AppJsonValue{AppJsonValue::String, false, 0.0, href, {}, {}};
        return make_element("link", text, opts, {}, "{\"color\":\"#2563eb\",\"textDecoration\":\"underline\"}");
    }
    if (functionName == "heading" || functionName == "header") {
        std::string text = args.size() > 0 ? args[0].to_string() : "";
        int level = args.size() > 1 ? args[1].to_int(1) : 1;
        if (level < 1) level = 1;
        if (level > 6) level = 6;
        AppJsonValue opts = args.size() > 2 ? args[2] : AppJsonValue{};
        return make_element("h" + std::to_string(level), text, opts, {}, "{\"margin\":\"0\",\"fontWeight\":\"bold\"}");
    }
    if (functionName == "divider" || functionName == "separator") {
        AppJsonValue opts = args.size() > 0 ? args[0] : AppJsonValue{};
        return make_element("hr", "", opts, {}, "{\"border\":\"none\",\"borderTop\":\"1px solid #e4e4e7\",\"margin\":\"12px 0\"}");
    }
    if (functionName == "row") {
        std::vector<AppJsonValue> children;
        if (args.size() > 0 && args[0].type == AppJsonValue::Array) children = args[0].arr;
        AppJsonValue opts = args.size() > 1 ? args[1] : AppJsonValue{};
        return make_element("row", "", opts, children, "{\"display\":\"flex\",\"flexDirection\":\"row\",\"gap\":\"8px\"}");
    }
    if (functionName == "column") {
        std::vector<AppJsonValue> children;
        if (args.size() > 0 && args[0].type == AppJsonValue::Array) children = args[0].arr;
        AppJsonValue opts = args.size() > 1 ? args[1] : AppJsonValue{};
        return make_element("column", "", opts, children, "{\"display\":\"flex\",\"flexDirection\":\"column\",\"gap\":\"8px\"}");
    }
    if (functionName == "box" || functionName == "card" || functionName == "div") {
        std::vector<AppJsonValue> children;
        if (args.size() > 0 && args[0].type == AppJsonValue::Array) children = args[0].arr;
        AppJsonValue opts = args.size() > 1 ? args[1] : AppJsonValue{};
        return make_element("box", "", opts, children, "{\"background\":\"#ffffff\",\"border\":\"1px solid #e4e4e7\",\"radius\":6,\"padding\":\"16px\"}");
    }
    if (functionName == "element") {
        std::string tag = args.size() > 0 ? args[0].to_string() : "div";
        AppJsonValue props = args.size() > 1 ? args[1] : AppJsonValue{};
        std::vector<AppJsonValue> children;
        if (args.size() > 2 && args[2].type == AppJsonValue::Array) children = args[2].arr;
        std::string style = "{}";
        if (args.size() > 3) style = serialize_json_val(args[3]);
        return make_element(tag, "", props, children, style);
    }
    if (functionName == "to_html" || functionName == "toHtml") {
        if (args.empty()) return "\"\"";
        return "\"" + escape_json(render_html(args[0])) + "\"";
    }
    if (functionName == "to_slint" || functionName == "toSlint") {
        if (args.empty()) return "\"\"";
        std::string title = "RayQuiro Application";
        int width = 960;
        int height = 680;
        AppJsonValue root = args[0];
        if (args.size() > 1 && args[1].type == AppJsonValue::Object) {
            auto itT = args[1].obj.find("title");
            if (itT != args[1].obj.end()) title = itT->second.to_string();
            auto itW = args[1].obj.find("width");
            if (itW != args[1].obj.end()) width = itW->second.to_int(960);
            auto itH = args[1].obj.find("height");
            if (itH != args[1].obj.end()) height = itH->second.to_int(680);
        }
        return "\"" + escape_json(compile_element_to_slint_app(root, title, width, height)) + "\"";
    }

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
#else
    if (functionName == "window") {
        return "{\"__type__\":\"Window\",\"id\":1,\"title\":\"RayQuiro\",\"width\":800,\"height\":600,\"open\":true}";
    }
    if (functionName == "window_is_open" || functionName == "isOpen") return "false";
    if (functionName == "window_poll_events" || functionName == "pollEvents") return "null";
    if (functionName == "window_set_title" || functionName == "setTitle") return "null";
    if (functionName == "window_set_size" || functionName == "setSize") return "null";
    if (functionName == "window_close" || functionName == "close") return "null";
    if (functionName == "quit") {
        runtime().close();
        return "null";
    }
#endif

    if (functionName == "init") {
        runtime().init(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_int(900) : 900, args.size() > 2 ? args[2].to_int(600) : 600);
        return "null";
    }
    if (functionName == "style") {
        if (!args.empty()) runtime().set_style(args[0].to_string());
        return "null";
    }
    if (functionName == "theme" || functionName == "set_theme" || functionName == "setTheme") {
        if (!args.empty()) runtime().set_theme(args[0].to_string());
        return "null";
    }
    if (functionName == "native_button") {
        if (args.size() < 5) return "null";
        runtime().add_button(args[0].to_string(), args[1].to_int(), args[2].to_int(), args[3].to_int(), args[4].to_int());
        return "null";
    }
    if (functionName == "native_text") {
        if (args.size() < 5) return "null";
        runtime().add_text(args[0].to_string(), args[1].to_int(), args[2].to_int(), args[3].to_int(), args[4].to_int());
        return "null";
    }
    if (functionName == "msg") {
        RayQuiroApp::show_message(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_string() : "");
        return "null";
    }
    if (functionName == "has_slint" || functionName == "slint_available") {
        std::filesystem::path localTool = std::filesystem::current_path() / "tools" / "slint" / "slint-viewer.exe";
        if (std::filesystem::exists(localTool) || std::filesystem::exists("tools/slint/slint-viewer.exe")) return "true";
        return std::system("where slint-viewer >nul 2>&1") == 0 ? "true" : "false";
    }
    if (functionName == "run" || functionName == "studio" || functionName == "desktop" || functionName == "slint" || functionName == "slint_run" || functionName == "run_slint" || functionName == "slint_launch" || functionName == "slint_check" || functionName == "slint_screenshot") {
        std::filesystem::path localTool = std::filesystem::current_path() / "tools" / "slint" / "slint-viewer.exe";
        bool hasSlintTool = std::filesystem::exists(localTool) || std::filesystem::exists("tools/slint/slint-viewer.exe") || (std::system("where slint-viewer >nul 2>&1") == 0);
        if (!hasSlintTool && functionName == "run") {
            runtime().run();
            return "null";
        }

        std::string input;
        if (!args.empty()) {
            if (args[0].type == AppJsonValue::String) {
                input = args[0].to_string();
            } else if (args[0].type == AppJsonValue::Object) {
                auto itType = args[0].obj.find("__type__");
                auto itTag = args[0].obj.find("tag");
                if ((itType != args[0].obj.end() && itType->second.to_string() == "Element") || itTag != args[0].obj.end()) {
                    input = compile_element_to_slint_app(args[0], "RayQuiro Application", 960, 680);
                } else {
                    std::string title = "RayQuiro Application";
                    int width = 960;
                    int height = 680;
                    auto itT = args[0].obj.find("title");
                    if (itT != args[0].obj.end()) title = itT->second.to_string();
                    auto itW = args[0].obj.find("width");
                    if (itW != args[0].obj.end()) width = itW->second.to_int(960);
                    auto itH = args[0].obj.find("height");
                    if (itH != args[0].obj.end()) height = itH->second.to_int(680);
                    auto itC = args[0].obj.find("content");
                    if (itC == args[0].obj.end()) itC = args[0].obj.find("root");
                    if (itC == args[0].obj.end()) itC = args[0].obj.find("body");
                    if (itC != args[0].obj.end()) {
                        input = compile_element_to_slint_app(itC->second, title, width, height);
                    }
                }
            }
        }
        if (input.empty()) {
            input = g_default_slint_app;
        }

        std::string slintPath;
        bool isTemp = false;
        std::filesystem::path tempFilePath;
        if (input.size() > 6 && input.substr(input.size() - 6) == ".slint" && std::filesystem::exists(input)) {
            slintPath = input;
        } else {
            tempFilePath = std::filesystem::temp_directory_path() / "rayquiro_studio_ui.slint";
            slintPath = tempFilePath.string();
            std::ofstream f(slintPath);
            f << input;
            f.close();
            isTemp = true;
        }

        std::string viewerExe = "slint-viewer";
        if (std::filesystem::exists(localTool)) {
            viewerExe = "\"" + localTool.string() + "\"";
        } else if (std::filesystem::exists("tools/slint/slint-viewer.exe")) {
            viewerExe = "\"tools\\slint\\slint-viewer.exe\"";
        }

        bool asyncLaunch = (functionName == "slint_launch");
        std::string extraArgs;
        if (functionName == "slint_check") {
            extraArgs = " --check";
        } else if (functionName == "slint_screenshot") {
            std::string outImg = args.size() > 1 ? args[1].to_string() : "screenshot.png";
            extraArgs = " --screenshot \"" + outImg + "\"";
        } else {
            for (size_t i = 1; i < args.size(); ++i) {
                extraArgs += " " + args[i].to_string();
            }
        }

        std::string cmd;
        if (asyncLaunch) {
            cmd = "cmd /C start \"\" " + viewerExe + " \"" + slintPath + "\"" + extraArgs;
        } else {
#ifdef _WIN32
            cmd = "cmd /C \"" + viewerExe + " \"" + slintPath + "\"" + extraArgs + "\"";
#else
            cmd = viewerExe + " \"" + slintPath + "\"" + extraArgs;
#endif
        }

        int ret = std::system(cmd.c_str());
        if (isTemp && !asyncLaunch) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
        }
        return ret == 0 ? "true" : "false";
    }

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
