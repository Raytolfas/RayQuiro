#include "BuiltinModules.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

namespace {
struct WebJsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<WebJsonValue> arr;
    std::unordered_map<std::string, WebJsonValue> obj;

    WebJsonValue() = default;
    WebJsonValue(double n) : type(Number), num(n) {}
    WebJsonValue(std::string s) : type(String), str(std::move(s)) {}
    WebJsonValue(const char* s) : type(String), str(s ? s : "") {}
    WebJsonValue(bool val) : type(Bool), b(val) {}

    bool is_null() const { return type == Null; }
    bool is_bool() const { return type == Bool; }
    bool is_number() const { return type == Number; }
    bool is_string() const { return type == String; }
    bool is_array() const { return type == Array; }
    bool is_object() const { return type == Object; }

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

using ScalarValue = WebJsonValue;

struct WebRoute {
    std::string route = "/";
    std::string title = "RayQuiro";
    std::string path;
    std::string head;
    std::string body;
    std::vector<std::string> stack;
};

struct WebState {
    std::string defaultTitle = "RayQuiro";
    std::string outputRoot = "build";
    std::string publicDir = "public";
    std::string css;
    std::unordered_map<std::string, WebRoute> routes;
    std::string currentRoute = "/";
    bool active = false;
    bool liveMode = false;
    int livePort = 5274;
};

WebState& state() {
    static WebState value;
    return value;
}

std::string trim(const std::string& value) {
    const auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

std::string to_string(const ScalarValue& value) {
    return value.to_string();
}

double to_number(const ScalarValue& value) {
    return value.to_number();
}

std::string json_escape(const std::string& value) {
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

std::string json_stringify(const ScalarValue& value) {
    if (value.type == ScalarValue::Null) return "null";
    if (value.type == ScalarValue::String) return "\"" + json_escape(value.str) + "\"";
    if (value.type == ScalarValue::Bool) return value.b ? "true" : "false";
    if (value.type == ScalarValue::Number) {
        std::ostringstream out;
        out << value.num;
        return out.str();
    }
    if (value.type == ScalarValue::Array) {
        std::string res = "[";
        for (size_t i = 0; i < value.arr.size(); ++i) {
            if (i > 0) res += ",";
            res += json_stringify(value.arr[i]);
        }
        res += "]";
        return res;
    }
    if (value.type == ScalarValue::Object) {
        std::string res = "{";
        bool first = true;
        for (const auto& kv : value.obj) {
            if (!first) res += ",";
            first = false;
            res += "\"" + json_escape(kv.first) + "\":" + json_stringify(kv.second);
        }
        res += "}";
        return res;
    }
    return "null";
}

void json_skip_ws(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) ++pos;
}

std::string json_parse_string(const std::string& text, size_t& pos) {
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

ScalarValue json_parse_value(const std::string& text, size_t& pos);

ScalarValue json_parse_object(const std::string& text, size_t& pos) {
    ScalarValue val;
    val.type = ScalarValue::Object;
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
        ScalarValue fieldVal = json_parse_value(text, pos);
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

ScalarValue json_parse_array(const std::string& text, size_t& pos) {
    ScalarValue val;
    val.type = ScalarValue::Array;
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

ScalarValue json_parse_value(const std::string& text, size_t& pos) {
    json_skip_ws(text, pos);
    if (pos >= text.size()) {
        throw std::runtime_error("Unexpected end of JSON.");
    }
    if (text[pos] == '{') return json_parse_object(text, pos);
    if (text[pos] == '[') return json_parse_array(text, pos);
    if (text[pos] == '"') {
        return ScalarValue(json_parse_string(text, pos));
    }
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        return ScalarValue(true);
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        return ScalarValue(false);
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        return ScalarValue();
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
        throw std::runtime_error("Invalid JSON scalar.");
    }
    const double number = std::stod(text.substr(pos, end - pos));
    pos = end;
    return ScalarValue(number);
}

ScalarValue json_parse_scalar(const std::string& text, size_t& pos) {
    return json_parse_value(text, pos);
}

std::vector<ScalarValue> parse_args(const char* jsonArgs) {
    const std::string text = jsonArgs ? jsonArgs : "[]";
    size_t pos = 0;
    json_skip_ws(text, pos);
    if (pos >= text.size() || text[pos] != '[') {
        throw std::runtime_error("Native module args must be a JSON array.");
    }
    ++pos;

    std::vector<ScalarValue> result;
    json_skip_ws(text, pos);
    if (pos < text.size() && text[pos] == ']') {
        ++pos;
        return result;
    }

    while (pos < text.size()) {
        result.push_back(json_parse_scalar(text, pos));
        json_skip_ws(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            break;
        }
        throw std::runtime_error("Invalid JSON array payload.");
    }
    return result;
}

char* duplicate_string(const std::string& value) {
    char* memory = new char[value.size() + 1];
    std::memcpy(memory, value.c_str(), value.size() + 1);
    return memory;
}

std::string default_css() {
    return R"CSS(:root {
  color-scheme: light;
  font-family: "Segoe UI", sans-serif;
  background: #f5f8ff;
  color: #0f172a;
}
body {
  margin: 0;
  min-height: 100vh;
  background:
    radial-gradient(circle at top left, rgba(34, 197, 94, 0.12), transparent 28%),
    linear-gradient(160deg, #f8fbff 0%, #e8f0ff 100%);
}
a { color: inherit; text-decoration: none; }
.page {
  width: min(1040px, calc(100% - 48px));
  margin: 0 auto;
  padding: 72px 0;
}
.hero {
  display: grid;
  gap: 20px;
  background: rgba(255,255,255,0.78);
  border: 1px solid rgba(15, 23, 42, 0.08);
  box-shadow: 0 24px 60px rgba(15, 23, 42, 0.12);
  border-radius: 28px;
  padding: 36px;
  backdrop-filter: blur(12px);
}
.eyebrow {
  letter-spacing: 0.24em;
  text-transform: uppercase;
  font-size: 12px;
  color: #2563eb;
}
.title {
  font-size: clamp(38px, 8vw, 72px);
  line-height: 0.98;
  margin: 0;
}
.lead {
  font-size: 18px;
  line-height: 1.7;
  color: #334155;
}
.primary, .button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 999px;
  padding: 14px 22px;
  font-weight: 700;
  background: linear-gradient(135deg, #0f172a 0%, #2563eb 100%);
  color: #ffffff;
  box-shadow: 0 14px 30px rgba(37, 99, 235, 0.25);
})CSS";
}

std::string class_attr(const std::string& value) {
    return value.empty() ? "" : " class=\"" + value + "\"";
}

std::string normalize_route(const std::string& raw) {
    std::string route = trim(raw);
    if (route.empty()) return "/";
    if (route.front() != '/') route = "/" + route;
    while (route.size() > 1 && route.back() == '/') route.pop_back();
    return route;
}

std::string route_output_path(const WebState& stateValue, const std::string& routeValue) {
    const std::string route = normalize_route(routeValue);
    const std::filesystem::path root = stateValue.outputRoot.empty() ? std::filesystem::path("build") : std::filesystem::path(stateValue.outputRoot);
    if (route == "/") {
        return (root / "index.html").string();
    }
    return (root / route.substr(1) / "index.html").string();
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void replace_all(std::string& text, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) return;
    size_t start = 0;
    while ((start = text.find(needle, start)) != std::string::npos) {
        text.replace(start, needle.size(), replacement);
        start += replacement.size();
    }
}

std::filesystem::path template_path(const WebState& stateValue) {
    const std::filesystem::path candidate = std::filesystem::path(stateValue.publicDir) / "index.html";
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
        return candidate;
    }
    return {};
}

WebRoute& current_route() {
    auto& web = state();
    const std::string key = web.currentRoute.empty() ? std::string("/") : web.currentRoute;
    auto found = web.routes.find(key);
    if (found == web.routes.end()) {
        WebRoute route;
        route.route = key;
        route.title = web.defaultTitle;
        route.path = route_output_path(web, key);
        found = web.routes.insert({key, route}).first;
    }
    return found->second;
}

void prepare_session() {
    auto& web = state();
    if (web.defaultTitle.empty()) web.defaultTitle = "RayQuiro";
    if (web.outputRoot.empty()) web.outputRoot = "build";
    if (web.publicDir.empty()) web.publicDir = "public";
    if (web.css.empty()) web.css = default_css();
    if (web.currentRoute.empty()) web.currentRoute = "/";
    web.active = true;

    auto& route = current_route();
    if (route.route.empty()) route.route = "/";
    if (route.title.empty()) route.title = web.defaultTitle;
    if (route.path.empty()) route.path = route_output_path(web, route.route);
}

void push_tag(const std::string& tag) {
    current_route().stack.push_back(tag);
}

std::string pop_tag(const std::string& fallback) {
    auto& route = current_route();
    if (route.stack.empty()) return fallback;
    const std::string tag = route.stack.back();
    route.stack.pop_back();
    return tag;
}

std::string extract_tag_name(const std::string& markup) {
    const std::string stripped = trim(markup);
    if (stripped.size() < 3 || stripped[0] != '<' || stripped[1] == '/' || stripped[1] == '!') return "";
    size_t start = 1;
    while (start < stripped.size() && std::isspace(static_cast<unsigned char>(stripped[start])) != 0) ++start;
    size_t end = start;
    while (end < stripped.size()) {
        const char ch = stripped[end];
        if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '>' || ch == '/') break;
        ++end;
    }
    if (end <= start) return "";
    return stripped.substr(start, end - start);
}

bool should_push_markup_tag(const std::string& markup) {
    const std::string stripped = trim(markup);
    if (stripped.empty() || stripped[0] != '<') return false;
    if (stripped.size() >= 2 && stripped[1] == '/') return false;
    if (stripped.find("/>") != std::string::npos) return false;
    if (stripped.find("</") != std::string::npos) return false;
    return !extract_tag_name(stripped).empty();
}

void copy_public_assets(const WebState& web) {
    const std::filesystem::path publicRoot(web.publicDir);
    if (!std::filesystem::exists(publicRoot) || !std::filesystem::is_directory(publicRoot)) return;

    const std::filesystem::path outputRoot(web.outputRoot);
    std::filesystem::create_directories(outputRoot);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot)) {
        if (!entry.is_regular_file()) continue;
        const std::filesystem::path relative = std::filesystem::relative(entry.path(), publicRoot);
        const std::filesystem::path target = outputRoot / relative;
        const auto parent = target.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing);
    }
}

std::string live_script() {
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

std::string render_document(const WebState& web, const WebRoute& route) {
    const std::string titleBlock = "<title>" + route.title + "</title>";
    const std::string styleBlock = "<style>" + web.css + "</style>";
    const std::string headBlock = route.head;
    const std::string liveBlock = web.liveMode ? live_script() : "";
    const std::string moduleMarker = "";

    const std::filesystem::path tpl = template_path(web);
    if (!tpl.empty()) {
        std::string html = read_file(tpl);
        replace_all(html, "{{ rq_title }}", route.title);
        replace_all(html, "<!-- rq-styles -->", styleBlock);
        replace_all(html, "<!-- rq-head -->", titleBlock + styleBlock + headBlock);
        replace_all(html, "<!-- rq-body -->", moduleMarker + route.body);
        replace_all(html, "<!-- rq-live -->", liveBlock);
        if (html.find(titleBlock) == std::string::npos && html.find("</head>") != std::string::npos) {
            html.insert(html.find("</head>"), titleBlock + styleBlock + headBlock);
        }
        if (html.find(route.body) == std::string::npos && html.find("</body>") != std::string::npos) {
            html.insert(html.find("</body>"), moduleMarker + route.body + liveBlock);
        }
        return html;
    }

    std::string html = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += titleBlock;
    html += styleBlock;
    html += headBlock;
    html += "</head><body>";
    html += moduleMarker;
    html += route.body;
    html += liveBlock;
    html += "</body></html>";
    return html;
}

void write_output(const WebState& web) {
    copy_public_assets(web);
    for (const auto& pair : web.routes) {
        const WebRoute& route = pair.second;
        const std::filesystem::path outPath(route.path);
        const auto parent = outPath.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        std::ofstream file(outPath, std::ios::binary | std::ios::trunc);
        file << render_document(web, route);
    }
}

std::string content_type_for(const std::filesystem::path& requested) {
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

void send_http(SOCKET client, const std::string& status, const std::string& contentType, const std::string& body) {
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

std::string file_version(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::exists(path)) return "0";
        return std::to_string(static_cast<long long>(std::filesystem::last_write_time(path).time_since_epoch().count()));
    } catch (...) {
        return "0";
    }
}

std::optional<std::filesystem::path> route_request_target(const WebState& web, const std::string& requestPath) {
    std::string path = requestPath;
    const size_t query = path.find('?');
    if (query != std::string::npos) path = path.substr(0, query);
    const std::string normalized = normalize_route(path);
    const auto found = web.routes.find(normalized);
    if (found == web.routes.end()) return std::nullopt;
    return std::filesystem::path(found->second.path);
}

std::optional<std::filesystem::path> static_request_target(const WebState& web, const std::string& requestPath) {
    std::string path = requestPath;
    const size_t query = path.find('?');
    if (query != std::string::npos) path = path.substr(0, query);
    if (path.empty() || path == "/") return std::nullopt;
    std::string relative = path[0] == '/' ? path.substr(1) : path;
    if (relative.find("..") != std::string::npos) return std::nullopt;
    const std::filesystem::path requested = std::filesystem::path(web.outputRoot) / relative;
    if (std::filesystem::exists(requested) && std::filesystem::is_regular_file(requested)) return requested;
    return std::nullopt;
}

void handle_client(SOCKET client, const WebState& web) {
    char buffer[4096];
    const int received = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) return;
    buffer[received] = '\0';

    std::istringstream request(std::string(buffer, received));
    std::string method;
    std::string path;
    request >> method >> path;

    const std::filesystem::path versionPath = std::filesystem::path(web.outputRoot) / "index.html";
    if (path == "/__rq_version") {
        send_http(client, "200 OK", "text/plain; charset=utf-8", file_version(versionPath));
        return;
    }
    if (const auto routeTarget = route_request_target(web, path)) {
        send_http(client, "200 OK", "text/html; charset=utf-8", read_file(*routeTarget));
        return;
    }
    if (const auto staticTarget = static_request_target(web, path)) {
        send_http(client, "200 OK", content_type_for(*staticTarget), read_file(*staticTarget));
        return;
    }
    send_http(client, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
}

void serve_forever(const WebState& web) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "[web] Failed to start Winsock for web.live()." << std::endl;
        return;
    }
#endif

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<u_short>(web.livePort));

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "[web] Port " << web.livePort << " is busy." << std::endl;
        closesocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    std::cout << "[web] Live web server: http://127.0.0.1:" << web.livePort << std::endl;
    std::cout << "[web] Press Ctrl+C to stop." << std::endl;

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        handle_client(client, web);
        closesocket(client);
    }

    closesocket(server);
#ifdef _WIN32
    WSACleanup();
#endif
}

ScalarValue call_begin(const std::vector<ScalarValue>& args) {
    auto& web = state();
    web = WebState{};
    web.defaultTitle = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const std::string requestedPath = args.size() > 1 ? to_string(args[1]) : "build/index.html";
    web.css = default_css();
    web.outputRoot = std::filesystem::path(requestedPath).parent_path().empty()
        ? std::string("build")
        : std::filesystem::path(requestedPath).parent_path().string();
    web.currentRoute = "/";
    web.active = true;

    WebRoute route;
    route.route = "/";
    route.title = web.defaultTitle;
    route.path = requestedPath;
    web.routes["/"] = route;
    return {};
}

ScalarValue call_route(const std::vector<ScalarValue>& args) {
    prepare_session();
    auto& web = state();
    const std::string routeKey = normalize_route(args.empty() ? "/" : to_string(args[0]));
    WebRoute& route = web.routes[routeKey];
    if (route.route.empty()) route.route = routeKey;
    if (route.title.empty()) route.title = web.defaultTitle;
    if (route.path.empty()) route.path = route_output_path(web, routeKey);
    if (args.size() > 1) route.title = to_string(args[1]);
    if (args.size() > 2) route.path = to_string(args[2]);
    web.currentRoute = routeKey;
    return ScalarValue(route.path);
}

ScalarValue call_head(const std::vector<ScalarValue>& args) {
    prepare_session();
    if (!args.empty()) current_route().head += to_string(args[0]);
    return {};
}

ScalarValue call_public(const std::vector<ScalarValue>& args) {
    prepare_session();
    auto& web = state();
    web.publicDir = args.empty() ? "public" : to_string(args[0]);
    return ScalarValue(web.publicDir);
}

ScalarValue call_live(const std::vector<ScalarValue>& args) {
    prepare_session();
    auto& web = state();
    web.liveMode = true;
    web.livePort = args.empty() ? 5274 : static_cast<int>(to_number(args[0]));
    if (web.livePort <= 0) web.livePort = 5274;
    return ScalarValue(static_cast<double>(web.livePort));
}

ScalarValue call_style(const std::vector<ScalarValue>& args) {
    if (args.empty()) return {};
    prepare_session();
    auto& web = state();
    if (!web.css.empty()) web.css += "\n";
    web.css += to_string(args[0]);
    return {};
}

ScalarValue call_open(const std::vector<ScalarValue>& args) {
    prepare_session();
    auto& route = current_route();
    const std::string first = args.size() > 0 ? to_string(args[0]) : "div";
    const std::string stripped = trim(first);
    if (!stripped.empty() && stripped[0] == '<') {
        route.body += first;
        if (should_push_markup_tag(stripped)) push_tag(extract_tag_name(stripped));
        return {};
    }
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    route.body += "<" + first + class_attr(className) + ">";
    push_tag(first);
    return {};
}

ScalarValue call_close(const std::vector<ScalarValue>& args) {
    prepare_session();
    const std::string fallback = args.size() > 0 ? to_string(args[0]) : "div";
    current_route().body += "</" + pop_tag(fallback) + ">";
    return {};
}

ScalarValue call_text(const std::vector<ScalarValue>& args) {
    prepare_session();
    auto& route = current_route();
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    const std::string tag = args.size() > 2 ? to_string(args[2]) : "span";
    route.body += "<" + tag + class_attr(className) + ">" + text + "</" + tag + ">";
    return {};
}

ScalarValue call_heading(const std::vector<ScalarValue>& args, const char* tag) {
    prepare_session();
    auto& route = current_route();
    const std::string text = args.size() > 0 ? to_string(args[0]) : "";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "";
    route.body += "<";
    route.body += tag;
    route.body += class_attr(className);
    route.body += ">";
    route.body += text;
    route.body += "</";
    route.body += tag;
    route.body += ">";
    return {};
}

ScalarValue call_button(const std::vector<ScalarValue>& args) {
    prepare_session();
    auto& route = current_route();
    const std::string label = args.size() > 0 ? to_string(args[0]) : "Button";
    const std::string className = args.size() > 1 ? to_string(args[1]) : "button";
    const std::string href = args.size() > 2 ? to_string(args[2]) : "#";
    route.body += "<a href=\"" + href + "\"" + class_attr(className) + ">" + label + "</a>";
    return {};
}

ScalarValue call_raw(const std::vector<ScalarValue>& args) {
    if (args.empty()) return {};
    prepare_session();
    current_route().body += to_string(args[0]);
    return {};
}

ScalarValue call_end(const std::vector<ScalarValue>&) {
    auto& web = state();
    for (auto& pair : web.routes) {
        while (!pair.second.stack.empty()) {
            pair.second.body += "</" + pair.second.stack.back() + ">";
            pair.second.stack.pop_back();
        }
    }
    write_output(web);
    web.active = false;
    if (web.liveMode) serve_forever(web);
    return {};
}

ScalarValue call_page(const std::vector<ScalarValue>& args) {
    auto& web = state();
    web = WebState{};
    web.defaultTitle = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
    const std::string requestedPath = args.size() > 2 ? to_string(args[2]) : "build/index.html";
    web.css = args.size() > 3 ? to_string(args[3]) : default_css();
    web.outputRoot = std::filesystem::path(requestedPath).parent_path().empty()
        ? std::string("build")
        : std::filesystem::path(requestedPath).parent_path().string();
    web.currentRoute = "/";

    WebRoute route;
    route.route = "/";
    route.title = web.defaultTitle;
    route.path = requestedPath;
    route.body = args.size() > 1 ? to_string(args[1]) : "";
    web.routes["/"] = route;
    write_output(web);
    return {};
}

ScalarValue dispatch(const std::string& functionName, const std::vector<ScalarValue>& args) {
    if (functionName == "begin") return call_begin(args);
    if (functionName == "route") return call_route(args);
    if (functionName == "head") return call_head(args);
    if (functionName == "public") return call_public(args);
    if (functionName == "live" || functionName == "serve") return call_live(args);
    if (functionName == "style") return call_style(args);
    if (functionName == "open") return call_open(args);
    if (functionName == "close") return call_close(args);
    if (functionName == "text") return call_text(args);
    if (functionName == "h1") return call_heading(args, "h1");
    if (functionName == "h2") return call_heading(args, "h2");
    if (functionName == "p") return call_heading(args, "p");
    if (functionName == "button") return call_button(args);
    if (functionName == "raw" || functionName == "html") return call_raw(args);
    if (functionName == "end") return call_end(args);
    if (functionName == "page") return call_page(args);
    throw std::runtime_error("rayquiro.web does not export web." + functionName);
}
}

int rqm_builtin_web_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message) {
    try {
        const std::vector<ScalarValue> args = parse_args(json_args);
        const ScalarValue result = dispatch(function_name == nullptr ? "" : std::string(function_name), args);
        if (json_result != nullptr) {
            *json_result = duplicate_string(json_stringify(result));
        }
        if (error_message != nullptr) {
            *error_message = nullptr;
        }
        return 0;
    } catch (const std::exception& error) {
        if (json_result != nullptr) {
            *json_result = duplicate_string("null");
        }
        if (error_message != nullptr) {
            *error_message = duplicate_string(error.what());
        }
        return 1;
    }
}

void rqm_builtin_free(char* memory) {
    delete[] memory;
}

