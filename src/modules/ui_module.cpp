#include "BuiltinModules.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "ModernUI.h"
#endif
#include "Framework.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
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

static std::string g_activeStyle;

char* duplicate_string(const std::string& value) {
    char* memory = new char[value.size() + 1];
    std::memcpy(memory, value.c_str(), value.size() + 1);
    return memory;
}

std::string escape_json(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
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
    if (pos >= text.size() || text[pos] != '{') throw std::runtime_error("Expected '{' for JSON object.");
    ++pos;
    while (pos < text.size()) {
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            return val;
        }
        std::string key = json_parse_string(text, pos);
        json_skip_ws(text, pos);
        if (pos >= text.size() || text[pos] != ':') throw std::runtime_error("Expected ':' after JSON key.");
        ++pos;
        AppJsonValue child = json_parse_value(text, pos);
        val.obj[key] = child;
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
    if (pos >= text.size() || text[pos] != '[') throw std::runtime_error("Expected '[' for JSON array.");
    ++pos;
    while (pos < text.size()) {
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            return val;
        }
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

static std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) --end;
    return value.substr(start, end - start);
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static bool parse_hex_color(const std::string& raw, int& r, int& g, int& b) {
    std::string hex = trim(raw);
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

static std::string get_theme_json(const std::string& rawName) {
    const std::string name = to_lower(trim(rawName));
    if (name == "light") {
        return "{\"name\":\"light\",\"background\":\"#ffffff\",\"surface\":\"#f4f4f5\",\"border\":\"#e4e4e7\",\"text\":\"#18181b\",\"textMuted\":\"#71717a\",\"primary\":\"#2563eb\",\"primaryHover\":\"#1d4ed8\",\"secondary\":\"#e2e8f0\",\"success\":\"#16a34a\",\"warning\":\"#ca8a04\",\"danger\":\"#dc2626\",\"radius\":8,\"font\":\"Segoe UI\"}";
    }
    if (name == "nord") {
        return "{\"name\":\"nord\",\"background\":\"#2e3440\",\"surface\":\"#3b4252\",\"border\":\"#434c5e\",\"text\":\"#eceff4\",\"textMuted\":\"#d8dee9\",\"primary\":\"#88c0d0\",\"primaryHover\":\"#81a1c1\",\"secondary\":\"#4c566a\",\"success\":\"#a3be8c\",\"warning\":\"#ebcb8b\",\"danger\":\"#bf616a\",\"radius\":8,\"font\":\"Segoe UI\"}";
    }
    if (name == "dracula") {
        return "{\"name\":\"dracula\",\"background\":\"#282a36\",\"surface\":\"#44475a\",\"border\":\"#6272a4\",\"text\":\"#f8f8f2\",\"textMuted\":\"#6272a4\",\"primary\":\"#bd93f9\",\"primaryHover\":\"#ff79c6\",\"secondary\":\"#44475a\",\"success\":\"#50fa7b\",\"warning\":\"#f1fa8c\",\"danger\":\"#ff5555\",\"radius\":8,\"font\":\"Segoe UI\"}";
    }
    if (name == "cyberpunk") {
        return "{\"name\":\"cyberpunk\",\"background\":\"#0a0a12\",\"surface\":\"#1a102f\",\"border\":\"#ff007f\",\"text\":\"#00ffcc\",\"textMuted\":\"#ff007f\",\"primary\":\"#ffe600\",\"primaryHover\":\"#fff04d\",\"secondary\":\"#7928ca\",\"success\":\"#00ff66\",\"warning\":\"#ffaa00\",\"danger\":\"#ff0055\",\"radius\":4,\"font\":\"Consolas\"}";
    }
    if (name == "midnight") {
        return "{\"name\":\"midnight\",\"background\":\"#090d16\",\"surface\":\"#111827\",\"border\":\"#1f2937\",\"text\":\"#f3f4f6\",\"textMuted\":\"#9ca3af\",\"primary\":\"#6366f1\",\"primaryHover\":\"#4f46e5\",\"secondary\":\"#374151\",\"success\":\"#10b981\",\"warning\":\"#f59e0b\",\"danger\":\"#ef4444\",\"radius\":10,\"font\":\"Segoe UI\"}";
    }
    return "{\"name\":\"dark\",\"background\":\"#18181b\",\"surface\":\"#27272a\",\"border\":\"#3f3f46\",\"text\":\"#f4f4f5\",\"textMuted\":\"#a1a1aa\",\"primary\":\"#3b82f6\",\"primaryHover\":\"#2563eb\",\"secondary\":\"#64748b\",\"success\":\"#22c55e\",\"warning\":\"#eab308\",\"danger\":\"#ef4444\",\"radius\":8,\"font\":\"Segoe UI\"}";
}

static std::string create_theme_json(const AppJsonValue& options) {
    std::string base = "dark";
    if (options.type == AppJsonValue::Object) {
        auto it = options.obj.find("base");
        if (it != options.obj.end()) base = it->second.to_string();
    }
    size_t pos = 0;
    AppJsonValue baseTheme = json_parse_value(get_theme_json(base), pos);
    if (options.type == AppJsonValue::Object) {
        for (const auto& pair : options.obj) {
            if (pair.first == "base") continue;
            baseTheme.obj[pair.first] = pair.second;
        }
    }
    std::string out = "{";
    bool first = true;
    for (const auto& pair : baseTheme.obj) {
        if (!first) out += ",";
        first = false;
        out += "\"" + escape_json(pair.first) + "\":";
        if (pair.second.type == AppJsonValue::Number) {
            out += pair.second.to_string();
        } else if (pair.second.type == AppJsonValue::Bool) {
            out += pair.second.b ? "true" : "false";
        } else {
            out += "\"" + escape_json(pair.second.to_string()) + "\"";
        }
    }
    out += "}";
    return out;
}

static std::string parse_css_to_json(const std::string& css) {
    std::string out = "{";
    bool firstRule = true;
    size_t cursor = 0;
    while (cursor < css.size()) {
        size_t selectorStart = css.find_first_not_of(" \t\r\n", cursor);
        if (selectorStart == std::string::npos) break;
        size_t braceOpen = css.find('{', selectorStart);
        if (braceOpen == std::string::npos) break;
        size_t braceClose = css.find('}', braceOpen + 1);
        if (braceClose == std::string::npos) break;

        std::string selector = trim(css.substr(selectorStart, braceOpen - selectorStart));
        std::string body = css.substr(braceOpen + 1, braceClose - braceOpen - 1);

        if (!firstRule) out += ",";
        firstRule = false;
        out += "\"" + escape_json(selector) + "\":{";

        std::stringstream ss(body);
        std::string decl;
        bool firstProp = true;
        while (std::getline(ss, decl, ';')) {
            size_t colon = decl.find(':');
            if (colon == std::string::npos) continue;
            std::string prop = trim(decl.substr(0, colon));
            std::string val = trim(decl.substr(colon + 1));
            if (prop.empty()) continue;
            if (!firstProp) out += ",";
            firstProp = false;
            out += "\"" + escape_json(prop) + "\":\"" + escape_json(val) + "\"";
        }
        out += "}";
        cursor = braceClose + 1;
    }
    out += "}";
    return out;
}

static std::string stringify_json_to_css(const AppJsonValue& val) {
    if (val.type != AppJsonValue::Object) return "\"\"";
    std::string css;
    for (const auto& selPair : val.obj) {
        css += selPair.first + " {\n";
        if (selPair.second.type == AppJsonValue::Object) {
            for (const auto& propPair : selPair.second.obj) {
                css += "  " + propPair.first + ": " + propPair.second.to_string() + ";\n";
            }
        }
        css += "}\n";
    }
    return "\"" + escape_json(css) + "\"";
}

static std::string button_style_json(const std::string& rawVariant, const AppJsonValue& opts) {
    std::string variant = to_lower(trim(rawVariant));
    std::string bg = "#3b82f6";
    std::string fg = "#ffffff";
    std::string border = "#3b82f6";
    int radius = 8;
    std::string padding = "8px 16px";
    std::string font = "Segoe UI";

    if (variant == "secondary") {
        bg = "#27272a";
        fg = "#f4f4f5";
        border = "#3f3f46";
    } else if (variant == "danger") {
        bg = "#ef4444";
        fg = "#ffffff";
        border = "#dc2626";
    } else if (variant == "success") {
        bg = "#22c55e";
        fg = "#ffffff";
        border = "#16a34a";
    } else if (variant == "ghost") {
        bg = "transparent";
        fg = "#f4f4f5";
        border = "transparent";
    } else if (variant == "outline") {
        bg = "transparent";
        fg = "#3b82f6";
        border = "#3b82f6";
    }

    if (opts.type == AppJsonValue::Object) {
        auto it = opts.obj.find("background");
        if (it != opts.obj.end()) bg = it->second.to_string();
        it = opts.obj.find("foreground");
        if (it != opts.obj.end()) fg = it->second.to_string();
        it = opts.obj.find("border");
        if (it != opts.obj.end()) border = it->second.to_string();
        it = opts.obj.find("radius");
        if (it != opts.obj.end()) radius = it->second.to_int(radius);
        it = opts.obj.find("padding");
        if (it != opts.obj.end()) padding = it->second.to_string();
        it = opts.obj.find("font");
        if (it != opts.obj.end()) font = it->second.to_string();
    }

    return "{\"variant\":\"" + escape_json(variant) + "\",\"background\":\"" + escape_json(bg) + "\",\"foreground\":\"" + escape_json(fg) + "\",\"border\":\"" + escape_json(border) + "\",\"radius\":" + std::to_string(radius) + ",\"padding\":\"" + escape_json(padding) + "\",\"font\":\"" + escape_json(font) + "\"}";
}

static std::string card_style_json(const AppJsonValue& opts) {
    std::string bg = "#27272a";
    std::string border = "#3f3f46";
    int radius = 12;
    std::string padding = "20px";
    std::string shadow = "0 4px 6px -1px rgba(0,0,0,0.3)";

    if (opts.type == AppJsonValue::Object) {
        auto it = opts.obj.find("background");
        if (it != opts.obj.end()) bg = it->second.to_string();
        it = opts.obj.find("border");
        if (it != opts.obj.end()) border = it->second.to_string();
        it = opts.obj.find("radius");
        if (it != opts.obj.end()) radius = it->second.to_int(radius);
        it = opts.obj.find("padding");
        if (it != opts.obj.end()) padding = it->second.to_string();
        it = opts.obj.find("shadow");
        if (it != opts.obj.end()) shadow = it->second.to_string();
    }

    return "{\"background\":\"" + escape_json(bg) + "\",\"border\":\"" + escape_json(border) + "\",\"radius\":" + std::to_string(radius) + ",\"padding\":\"" + escape_json(padding) + "\",\"shadow\":\"" + escape_json(shadow) + "\"}";
}

static std::string text_style_json(const std::string& rawRole, const AppJsonValue& opts) {
    std::string role = to_lower(trim(rawRole));
    std::string color = "#f4f4f5";
    int size = 16;
    std::string weight = "normal";
    std::string font = "Segoe UI";

    if (role == "title") {
        size = 28;
        weight = "bold";
    } else if (role == "subtitle") {
        size = 20;
        weight = "600";
        color = "#a1a1aa";
    } else if (role == "caption") {
        size = 12;
        color = "#71717a";
    } else if (role == "code") {
        size = 14;
        font = "Consolas";
    }

    if (opts.type == AppJsonValue::Object) {
        auto it = opts.obj.find("color");
        if (it != opts.obj.end()) color = it->second.to_string();
        it = opts.obj.find("size");
        if (it != opts.obj.end()) size = it->second.to_int(size);
        it = opts.obj.find("weight");
        if (it != opts.obj.end()) weight = it->second.to_string();
        it = opts.obj.find("font");
        if (it != opts.obj.end()) font = it->second.to_string();
    }

    return "{\"role\":\"" + escape_json(role) + "\",\"color\":\"" + escape_json(color) + "\",\"size\":" + std::to_string(size) + ",\"weight\":\"" + escape_json(weight) + "\",\"font\":\"" + escape_json(font) + "\"}";
}

static std::string badge_style_json(const std::string& rawVariant, const AppJsonValue& opts) {
    std::string variant = to_lower(trim(rawVariant));
    std::string bg = "#3b82f6";
    std::string color = "#ffffff";
    int radius = 9999;
    std::string padding = "4px 8px";

    if (variant == "success") {
        bg = "#16a34a";
    } else if (variant == "warning") {
        bg = "#ca8a04";
    } else if (variant == "danger") {
        bg = "#dc2626";
    } else if (variant == "secondary") {
        bg = "#3f3f46";
    }

    if (opts.type == AppJsonValue::Object) {
        auto it = opts.obj.find("background");
        if (it != opts.obj.end()) bg = it->second.to_string();
        it = opts.obj.find("color");
        if (it != opts.obj.end()) color = it->second.to_string();
        it = opts.obj.find("radius");
        if (it != opts.obj.end()) radius = it->second.to_int(radius);
        it = opts.obj.find("padding");
        if (it != opts.obj.end()) padding = it->second.to_string();
    }

    return "{\"variant\":\"" + escape_json(variant) + "\",\"background\":\"" + escape_json(bg) + "\",\"color\":\"" + escape_json(color) + "\",\"radius\":" + std::to_string(radius) + ",\"padding\":\"" + escape_json(padding) + "\"}";
}

static std::string input_style_json(const AppJsonValue& opts) {
    std::string bg = "#18181b";
    std::string border = "#3f3f46";
    std::string borderFocus = "#3b82f6";
    std::string color = "#f4f4f5";
    std::string placeholder = "#71717a";
    int radius = 6;
    std::string padding = "8px 12px";

    if (opts.type == AppJsonValue::Object) {
        auto it = opts.obj.find("background");
        if (it != opts.obj.end()) bg = it->second.to_string();
        it = opts.obj.find("border");
        if (it != opts.obj.end()) border = it->second.to_string();
        it = opts.obj.find("borderFocus");
        if (it != opts.obj.end()) borderFocus = it->second.to_string();
        it = opts.obj.find("color");
        if (it != opts.obj.end()) color = it->second.to_string();
        it = opts.obj.find("radius");
        if (it != opts.obj.end()) radius = it->second.to_int(radius);
        it = opts.obj.find("padding");
        if (it != opts.obj.end()) padding = it->second.to_string();
    }

    return "{\"background\":\"" + escape_json(bg) + "\",\"border\":\"" + escape_json(border) + "\",\"borderFocus\":\"" + escape_json(borderFocus) + "\",\"color\":\"" + escape_json(color) + "\",\"placeholder\":\"" + escape_json(placeholder) + "\",\"radius\":" + std::to_string(radius) + ",\"padding\":\"" + escape_json(padding) + "\"}";
}

static std::string serialize_json_val(const AppJsonValue& val) {
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

static std::string apply_style_to_element(const AppJsonValue& element, const AppJsonValue& style) {
    if (element.type != AppJsonValue::Object) return "null";
    AppJsonValue result = element;
    if (style.type == AppJsonValue::Object) {
        auto& curStyle = result.obj["style"];
        if (curStyle.type != AppJsonValue::Object) curStyle.type = AppJsonValue::Object;
        for (const auto& pair : style.obj) {
            curStyle.obj[pair.first] = pair.second;
        }
    } else if (style.type == AppJsonValue::String) {
        size_t pos = 0;
        AppJsonValue parsedSheet = json_parse_value(parse_css_to_json(style.str), pos);
        auto& curStyle = result.obj["style"];
        if (curStyle.type != AppJsonValue::Object) curStyle.type = AppJsonValue::Object;
        for (const auto& sel : parsedSheet.obj) {
            if (sel.second.type == AppJsonValue::Object) {
                for (const auto& prop : sel.second.obj) {
                    curStyle.obj[prop.first] = prop.second;
                }
            }
        }
    }
    return serialize_json_val(result);
}

static std::string get_default_style_for_tag(const std::string& rawTag) {
    std::string tag = to_lower(trim(rawTag));
    if (tag == "button") return "{\"background\":\"#f4f4f5\",\"color\":\"#18181b\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"6px 12px\",\"cursor\":\"pointer\"}";
    if (tag == "text" || tag == "label") return "{\"color\":\"#18181b\",\"fontSize\":14}";
    if (tag == "input") return "{\"background\":\"#ffffff\",\"color\":\"#18181b\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"6px 8px\"}";
    if (tag == "textarea") return "{\"background\":\"#ffffff\",\"color\":\"#18181b\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"8px\"}";
    if (tag == "checkbox" || tag == "radio") return "{\"display\":\"inline-flex\",\"alignItems\":\"center\",\"gap\":\"6px\"}";
    if (tag == "box" || tag == "card" || tag == "div") return "{\"background\":\"#ffffff\",\"border\":\"1px solid #e4e4e7\",\"radius\":6,\"padding\":\"16px\"}";
    if (tag == "slider") return "{\"width\":\"100%\",\"accentColor\":\"#3b82f6\"}";
    if (tag == "progress") return "{\"width\":\"100%\",\"height\":\"12px\",\"radius\":6}";
    if (tag == "select") return "{\"background\":\"#ffffff\",\"border\":\"1px solid #d4d4d8\",\"radius\":4,\"padding\":\"6px 10px\"}";
    return "{}";
}

std::string dispatch(const std::string& functionName, const std::vector<AppJsonValue>& args) {
    if (functionName == "style") {
        if (!args.empty()) {
            RayQuiroApp::instance().set_style(args[0].to_string());
        }
        return "null";
    }
    if (functionName == "set_theme" || functionName == "setTheme") {
        if (!args.empty()) {
            RayQuiroApp::instance().set_theme(args[0].to_string());
        }
        return "null";
    }
    if (functionName == "apply" || functionName == "set_style" || functionName == "setStyle") {
        if (args.size() < 2) return args.size() > 0 ? serialize_json_val(args[0]) : "null";
        return apply_style_to_element(args[0], args[1]);
    }
    if (functionName == "default_style" || functionName == "defaultStyle" || functionName == "clean_style") {
        return get_default_style_for_tag(args.empty() ? "div" : args[0].to_string());
    }
    if (functionName == "minimal" || functionName == "clean") {
        return "{\"name\":\"clean\",\"background\":\"#ffffff\",\"surface\":\"#fafafa\",\"border\":\"#e4e4e7\",\"text\":\"#18181b\",\"textMuted\":\"#71717a\",\"primary\":\"#18181b\",\"secondary\":\"#71717a\",\"radius\":4,\"font\":\"sans-serif\"}";
    }

    if (functionName == "theme") {
        return get_theme_json(args.empty() ? "dark" : args[0].to_string());
    }
    if (functionName == "dark") return get_theme_json("dark");
    if (functionName == "light") return get_theme_json("light");
    if (functionName == "nord") return get_theme_json("nord");
    if (functionName == "dracula") return get_theme_json("dracula");
    if (functionName == "cyberpunk") return get_theme_json("cyberpunk");
    if (functionName == "midnight") return get_theme_json("midnight");
    if (functionName == "create_theme" || functionName == "createTheme") {
        return create_theme_json(args.empty() ? AppJsonValue{} : args[0]);
    }

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

    if (functionName == "button_style" || functionName == "buttonStyle") {
        return button_style_json(args.empty() ? "primary" : args[0].to_string(), args.size() > 1 ? args[1] : AppJsonValue{});
    }
    if (functionName == "card_style" || functionName == "cardStyle") {
        return card_style_json(args.empty() ? AppJsonValue{} : args[0]);
    }
    if (functionName == "text_style" || functionName == "textStyle") {
        return text_style_json(args.empty() ? "body" : args[0].to_string(), args.size() > 1 ? args[1] : AppJsonValue{});
    }
    if (functionName == "badge_style" || functionName == "badgeStyle") {
        return badge_style_json(args.empty() ? "default" : args[0].to_string(), args.size() > 1 ? args[1] : AppJsonValue{});
    }
    if (functionName == "input_style" || functionName == "inputStyle") {
        return input_style_json(args.empty() ? AppJsonValue{} : args[0]);
    }

    if (functionName == "parse" || functionName == "parse_style" || functionName == "parseStyle") {
        return parse_css_to_json(args.empty() ? "" : args[0].to_string());
    }
    if (functionName == "stringify" || functionName == "stringify_style" || functionName == "stringifyStyle") {
        return stringify_json_to_css(args.empty() ? AppJsonValue{} : args[0]);
    }
    if (functionName == "style") {
        if (!args.empty()) {
            g_activeStyle = args[0].to_string();
#ifdef _WIN32
            runtime().style(g_activeStyle);
#endif
        }
        return "\"" + escape_json(g_activeStyle) + "\"";
    }

#ifdef _WIN32
    if (functionName == "init") {
        runtime().init(args.size() > 0 ? args[0].to_string() : "RayQuiro", args.size() > 1 ? args[1].to_int(920) : 920, args.size() > 2 ? args[2].to_int(620) : 620);
        return "null";
    }
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
