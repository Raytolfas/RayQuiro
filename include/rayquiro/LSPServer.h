#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <regex>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "nlohmann_json.h"
#include "Lexer.h"
#include "Parser.h"

class LSPServer {
    std::unordered_map<std::string, std::string> documents_;
    bool running_ = true;

    std::string readMessage() {
        std::string line;
        int contentLength = 0;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                break;
            }
            if (line.rfind("Content-Length:", 0) == 0) {
                std::string lenStr = line.substr(15);
                size_t start = lenStr.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    contentLength = std::stoi(lenStr.substr(start));
                }
            }
        }
        if (contentLength <= 0) {
            return "";
        }
        std::vector<char> buffer(contentLength);
        std::cin.read(buffer.data(), contentLength);
        return std::string(buffer.data(), contentLength);
    }

    void sendMessage(const dap::json& msg) {
        std::string body = msg.dump();
        std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        std::cout << header << body;
        std::cout.flush();
    }

    void sendResponse(const dap::json& id, const dap::json& result) {
        dap::json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        resp["result"] = result;
        sendMessage(resp);
    }

    void sendError(const dap::json& id, int code, const std::string& message) {
        dap::json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = id;
        dap::json err;
        err["code"] = code;
        err["message"] = message;
        resp["error"] = err;
        sendMessage(resp);
    }

    void sendNotification(const std::string& method, const dap::json& params) {
        dap::json notif;
        notif["jsonrpc"] = "2.0";
        notif["method"] = method;
        notif["params"] = params;
        sendMessage(notif);
    }

    void validateDocument(const std::string& uri) {
        auto it = documents_.find(uri);
        if (it == documents_.end()) return;
        const std::string& text = it->second;

        dap::json params;
        params["uri"] = uri;
        dap::json diagArray = dap::json::array();

        try {
            Lexer lexer(text);
            auto tokens = lexer.tokenize();
            Parser parser(tokens);
            auto program = parser.parse();
        } catch (const std::exception& e) {
            std::string msg = e.what();
            int errLine = 0;
            int errCol = 0;
            size_t atPos = msg.rfind(" at ");
            if (atPos != std::string::npos) {
                std::string coord = msg.substr(atPos + 4);
                size_t colon = coord.find(':');
                if (colon != std::string::npos) {
                    try {
                        errLine = std::stoi(coord.substr(0, colon)) - 1;
                        errCol = std::stoi(coord.substr(colon + 1)) - 1;
                    } catch (...) {}
                }
            }
            if (errLine < 0) errLine = 0;
            if (errCol < 0) errCol = 0;

            int endCol = errCol + 1;
            std::istringstream stream(text);
            std::string lineContent;
            int curL = 0;
            while (std::getline(stream, lineContent)) {
                if (curL == errLine) {
                    if (errCol >= 0 && errCol < static_cast<int>(lineContent.size())) {
                        int pos = errCol;
                        while (pos < static_cast<int>(lineContent.size()) &&
                               (std::isalnum(static_cast<unsigned char>(lineContent[pos])) || lineContent[pos] == '_')) {
                            pos++;
                        }
                        if (pos > errCol) endCol = pos;
                    }
                    break;
                }
                curL++;
            }

            dap::json diag;
            dap::json range;
            dap::json startPos;
            startPos["line"] = errLine;
            startPos["character"] = errCol;
            dap::json endPos;
            endPos["line"] = errLine;
            endPos["character"] = endCol;
            range["start"] = startPos;
            range["end"] = endPos;
            diag["range"] = range;
            diag["severity"] = 1;
            diag["source"] = "rayquiro";
            diag["message"] = msg;
            diagArray.push_back(diag);
        }

        params["diagnostics"] = diagArray;
        sendNotification("textDocument/publishDiagnostics", params);
    }

    std::string getLinePrefix(const std::string& text, int targetLine, int targetCol) {
        std::istringstream stream(text);
        std::string currentLine;
        int lineIdx = 0;
        while (std::getline(stream, currentLine)) {
            if (!currentLine.empty() && currentLine.back() == '\r') currentLine.pop_back();
            if (lineIdx == targetLine) {
                if (targetCol >= 0 && targetCol <= static_cast<int>(currentLine.size())) {
                    return currentLine.substr(0, targetCol);
                }
                return currentLine;
            }
            lineIdx++;
        }
        return "";
    }

    std::string getWordAtPosition(const std::string& text, int targetLine, int targetCol) {
        std::istringstream stream(text);
        std::string line;
        int lineIdx = 0;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (lineIdx == targetLine) {
                if (targetCol < 0 || targetCol > static_cast<int>(line.size())) return "";
                int start = targetCol;
                while (start > 0 && (std::isalnum(static_cast<unsigned char>(line[start - 1])) || line[start - 1] == '_' || line[start - 1] == '.')) {
                    start--;
                }
                int end = targetCol;
                while (end < static_cast<int>(line.size()) && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_')) {
                    end++;
                }
                if (start < end) return line.substr(start, end - start);
                return "";
            }
            lineIdx++;
        }
        return "";
    }

    std::string urlDecode(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '%' && i + 2 < in.size()) {
                char h1 = in[i + 1];
                char h2 = in[i + 2];
                auto hexVal = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                int v1 = hexVal(h1);
                int v2 = hexVal(h2);
                if (v1 >= 0 && v2 >= 0) {
                    out.push_back(static_cast<char>((v1 << 4) | v2));
                    i += 2;
                    continue;
                }
            }
            out.push_back(in[i]);
        }
        return out;
    }

    std::string uriToPath(const std::string& uri) {
        std::string dec = urlDecode(uri);
        std::string prefix = "file://";
        if (dec.rfind(prefix, 0) == 0) {
            std::string p = dec.substr(prefix.size());
#ifdef _WIN32
            while (!p.empty() && p[0] == '/') {
                if (p.size() >= 3 && std::isalpha(static_cast<unsigned char>(p[1])) && p[2] == ':') {
                    p = p.substr(1);
                    break;
                }
                if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
                    break;
                }
                p = p.substr(1);
            }
#endif
            return p;
        }
        return dec;
    }

    dap::json makeCompletionItem(const std::string& label, int kind, const std::string& detail, const std::string& doc = "") {
        dap::json item;
        item["label"] = label;
        item["kind"] = kind;
        if (!detail.empty()) item["detail"] = detail;
        if (!doc.empty()) {
            dap::json docObj;
            docObj["kind"] = "markdown";
            docObj["value"] = doc;
            item["documentation"] = docObj;
        }
        return item;
    }

    dap::json resolveMemberCompletions(const std::string& uri, const std::string& target) {
        dap::json items = dap::json::array();
        auto it = documents_.find(uri);
        const std::string docText = (it != documents_.end()) ? it->second : "";
        std::filesystem::path docPath = uriToPath(uri);
        std::filesystem::path docDir = docPath.parent_path();

        std::unordered_map<std::string, std::string> importedAliases;
        std::regex importRegex(R"(import\s+(?:["']([^"']+)["']|([A-Za-z0-9_.]+))(?:\s+as\s+([A-Za-z_][A-Za-z0-9_]*))?)");
        auto textBegin = docText.cbegin();
        std::smatch match;
        while (std::regex_search(textBegin, docText.cend(), match, importRegex)) {
            std::string mod = match[1].matched ? match[1].str() : match[2].str();
            std::string alias = match[3].matched ? match[3].str() : "";
            if (alias.empty()) {
                std::string base = mod;
                size_t dot = base.rfind('.');
                if (dot != std::string::npos) base = base.substr(dot + 1);
                importedAliases[base] = mod;
                importedAliases[mod] = mod;
            } else {
                importedAliases[alias] = mod;
            }
            textBegin = match.suffix().first;
        }

        std::string importedModule;
        auto aliasIt = importedAliases.find(target);
        if (aliasIt != importedAliases.end()) {
            importedModule = aliasIt->second;
        }

        std::string canonical = importedModule;
        if (canonical.rfind("rayquiro.", 0) == 0) canonical = canonical.substr(9);
        if (canonical == "raytolfas.engine") canonical = "engine";

        if (!importedModule.empty()) {
            std::vector<std::filesystem::path> searchPaths = {
                docDir / importedModule,
                docDir / (importedModule + ".rq"),
                docDir / "packages" / importedModule / "main.rq",
                docDir / "packages" / importedModule / "index.rq",
                docDir / "packages" / importedModule / (importedModule + ".rq"),
                std::filesystem::current_path() / "packages" / importedModule / "main.rq",
                std::filesystem::current_path() / "packages" / importedModule / "index.rq",
                std::filesystem::current_path() / "packages" / importedModule / (importedModule + ".rq")
            };

            bool foundExternalFile = false;
            for (const auto& sp : searchPaths) {
                std::error_code ec;
                if (std::filesystem::exists(sp, ec) && std::filesystem::is_regular_file(sp, ec)) {
                    std::ifstream f(sp);
                    if (f.is_open()) {
                        std::stringstream buffer;
                        buffer << f.rdbuf();
                        std::string content = buffer.str();
                        std::regex fnRegex(R"(fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\))");
                        std::smatch fnMatch;
                        auto cBegin = content.cbegin();
                        while (std::regex_search(cBegin, content.cend(), fnMatch, fnRegex)) {
                            std::string fnName = fnMatch[1].str();
                            std::string params = fnMatch[2].str();
                            items.push_back(makeCompletionItem(fnName, 3, target + "." + fnName + "(" + params + ")", "Exported function from " + importedModule));
                            cBegin = fnMatch.suffix().first;
                        }
                        foundExternalFile = true;
                        break;
                    }
                }
            }

            if (foundExternalFile && items.size() > 0) {
                return items;
            }
        }

        if (canonical == "telebot") {
            items.push_back(makeCompletionItem("init", 3, target + ".init(token) -> Bot", "Initialize a Telegram bot client instance"));
            items.push_back(makeCompletionItem("send_message", 3, target + ".send_message(chat_id, text) -> bool", "Send a text message to a chat"));
            items.push_back(makeCompletionItem("on_message", 3, target + ".on_message(bot, fn) -> null", "Register a handler for incoming messages"));
            items.push_back(makeCompletionItem("command", 3, target + ".command(bot, cmd, fn) -> null", "Register a command handler"));
            items.push_back(makeCompletionItem("polling", 3, target + ".polling(bot) -> null", "Start long polling for updates"));
            items.push_back(makeCompletionItem("get_updates", 3, target + ".get_updates(bot) -> array", "Fetch new pending updates"));
            items.push_back(makeCompletionItem("set_webhook", 3, target + ".set_webhook(bot, url) -> bool", "Set custom Telegram webhook URL"));
            return items;
        }

        if (canonical == "ui") {
            items.push_back(makeCompletionItem("theme", 3, target + ".theme(name) -> Object", "Get preconfigured theme: dark, light, nord, dracula, cyberpunk, midnight"));
            items.push_back(makeCompletionItem("dark", 3, target + ".dark() -> Object", "Get dark theme palette"));
            items.push_back(makeCompletionItem("light", 3, target + ".light() -> Object", "Get light theme palette"));
            items.push_back(makeCompletionItem("nord", 3, target + ".nord() -> Object", "Get Nord theme palette"));
            items.push_back(makeCompletionItem("dracula", 3, target + ".dracula() -> Object", "Get Dracula theme palette"));
            items.push_back(makeCompletionItem("cyberpunk", 3, target + ".cyberpunk() -> Object", "Get Cyberpunk theme palette"));
            items.push_back(makeCompletionItem("midnight", 3, target + ".midnight() -> Object", "Get Midnight theme palette"));
            items.push_back(makeCompletionItem("create_theme", 3, target + ".create_theme(options) -> Object", "Create custom theme extending a base palette"));
            items.push_back(makeCompletionItem("rgb", 3, target + ".rgb(r, g, b) -> String", "Create hex color from RGB integers"));
            items.push_back(makeCompletionItem("rgba", 3, target + ".rgba(r, g, b, a) -> String", "Create rgba color string"));
            items.push_back(makeCompletionItem("hex", 3, target + ".hex(color) -> String", "Normalize hex color string"));
            items.push_back(makeCompletionItem("blend", 3, target + ".blend(c1, c2, factor) -> String", "Linear blend between two colors"));
            items.push_back(makeCompletionItem("lighten", 3, target + ".lighten(color, percent) -> String", "Lighten color by percentage"));
            items.push_back(makeCompletionItem("darken", 3, target + ".darken(color, percent) -> String", "Darken color by percentage"));
            items.push_back(makeCompletionItem("alpha", 3, target + ".alpha(color, opacity) -> String", "Set color opacity"));
            items.push_back(makeCompletionItem("button_style", 3, target + ".button_style(variant, options) -> Object", "Get styled button configuration"));
            items.push_back(makeCompletionItem("card_style", 3, target + ".card_style(options) -> Object", "Get styled card container configuration"));
            items.push_back(makeCompletionItem("text_style", 3, target + ".text_style(role, options) -> Object", "Get typography text style configuration"));
            items.push_back(makeCompletionItem("badge_style", 3, target + ".badge_style(variant, options) -> Object", "Get badge styling configuration"));
            items.push_back(makeCompletionItem("input_style", 3, target + ".input_style(options) -> Object", "Get input control styling configuration"));
            items.push_back(makeCompletionItem("parse", 3, target + ".parse(cssString) -> Object", "Parse CSS-like stylesheet into object"));
            items.push_back(makeCompletionItem("stringify", 3, target + ".stringify(styleObject) -> String", "Format style object as CSS stylesheet"));
            items.push_back(makeCompletionItem("style", 3, target + ".style(cssString) -> String", "Set active UI stylesheet"));
            items.push_back(makeCompletionItem("apply", 3, target + ".apply(element, style) -> Element", "Apply style dictionary or CSS to element"));
            items.push_back(makeCompletionItem("default_style", 3, target + ".default_style(tag) -> Object", "Get clean default CSS style for element tag"));
            items.push_back(makeCompletionItem("minimal", 3, target + ".minimal() -> Object", "Get clean minimal CSS design tokens"));
            return items;
        }

        if (canonical == "app") {
            items.push_back(makeCompletionItem("window", 3, target + ".window(options) -> Window", "Create desktop application window"));
            items.push_back(makeCompletionItem("button", 3, target + ".button(text, options) -> Element", "Create button UI element (HTML button)"));
            items.push_back(makeCompletionItem("text", 3, target + ".text(text, options) -> Element", "Create text/span UI element"));
            items.push_back(makeCompletionItem("label", 3, target + ".label(text, options) -> Element", "Create label UI element"));
            items.push_back(makeCompletionItem("input", 3, target + ".input(placeholder, value, options) -> Element", "Create text input element"));
            items.push_back(makeCompletionItem("textarea", 3, target + ".textarea(placeholder, value, options) -> Element", "Create textarea multiline element"));
            items.push_back(makeCompletionItem("checkbox", 3, target + ".checkbox(label, checked, options) -> Element", "Create checkbox element"));
            items.push_back(makeCompletionItem("radio", 3, target + ".radio(label, group, checked, options) -> Element", "Create radio button element"));
            items.push_back(makeCompletionItem("slider", 3, target + ".slider(min, max, value, options) -> Element", "Create numeric range slider"));
            items.push_back(makeCompletionItem("progress", 3, target + ".progress(value, max, options) -> Element", "Create progress bar"));
            items.push_back(makeCompletionItem("select", 3, target + ".select(options, selectedIndex, options) -> Element", "Create dropdown select"));
            items.push_back(makeCompletionItem("image", 3, target + ".image(src, options) -> Element", "Create image element"));
            items.push_back(makeCompletionItem("link", 3, target + ".link(url, text, options) -> Element", "Create hyperlink element"));
            items.push_back(makeCompletionItem("heading", 3, target + ".heading(text, level, options) -> Element", "Create heading h1-h6 element"));
            items.push_back(makeCompletionItem("divider", 3, target + ".divider(options) -> Element", "Create divider line hr element"));
            items.push_back(makeCompletionItem("row", 3, target + ".row(children, options) -> Element", "Create horizontal flex container"));
            items.push_back(makeCompletionItem("column", 3, target + ".column(children, options) -> Element", "Create vertical flex container"));
            items.push_back(makeCompletionItem("box", 3, target + ".box(children, options) -> Element", "Create container box / card"));
            items.push_back(makeCompletionItem("card", 3, target + ".card(children, options) -> Element", "Create styled card container"));
            items.push_back(makeCompletionItem("element", 3, target + ".element(tag, props, children, style) -> Element", "Create generic custom DOM element"));
            items.push_back(makeCompletionItem("to_html", 3, target + ".to_html(element) -> String", "Serialize UI element tree to HTML string"));
            items.push_back(makeCompletionItem("quit", 3, target + ".quit() -> null", "Quit application loop"));
            return items;
        }

        if (canonical == "web") {
            items.push_back(makeCompletionItem("server", 3, target + ".server() -> object", "Create web server instance"));
            items.push_back(makeCompletionItem("page", 3, target + ".page(title, body) -> null", "Emit HTML page"));
            items.push_back(makeCompletionItem("begin", 3, target + ".begin(title) -> null", "Begin HTML site generation"));
            items.push_back(makeCompletionItem("route", 3, target + ".route(path, title) -> null", "Define web route"));
            items.push_back(makeCompletionItem("head", 3, target + ".head(html) -> null", "Append raw HTML to head"));
            items.push_back(makeCompletionItem("public", 3, target + ".public(dir) -> null", "Set public static assets directory"));
            items.push_back(makeCompletionItem("live", 3, target + ".live(port) -> null", "Start live dev server"));
            items.push_back(makeCompletionItem("style", 3, target + ".style(css) -> null", "Apply global stylesheet"));
            items.push_back(makeCompletionItem("text", 3, target + ".text(str) -> null", "Append text block"));
            items.push_back(makeCompletionItem("h1", 3, target + ".h1(str) -> null", "Append heading 1"));
            items.push_back(makeCompletionItem("h2", 3, target + ".h2(str) -> null", "Append heading 2"));
            items.push_back(makeCompletionItem("p", 3, target + ".p(str) -> null", "Append paragraph"));
            items.push_back(makeCompletionItem("button", 3, target + ".button(text) -> null", "Append HTML button"));
            items.push_back(makeCompletionItem("raw", 3, target + ".raw(html) -> null", "Append raw HTML snippet"));
            items.push_back(makeCompletionItem("html", 3, target + ".html(content) -> null", "Append raw HTML snippet"));
            items.push_back(makeCompletionItem("render", 3, target + ".render(element_or_template) -> String", "Declarative render component or template to HTML"));
            items.push_back(makeCompletionItem("element", 3, target + ".element(tag, props, children) -> Object", "Create declarative HTML element node"));
            items.push_back(makeCompletionItem("template", 3, target + ".template(html, data) -> String", "Interpolate data object into HTML template"));
            items.push_back(makeCompletionItem("end", 3, target + ".end() -> null", "Finalize web site build"));
            items.push_back(makeCompletionItem("get", 3, target + ".get(url, [headers]) -> object", "HTTP GET request"));
            items.push_back(makeCompletionItem("post", 3, target + ".post(url, body, [headers]) -> object", "HTTP POST request"));
            return items;
        }

        if (canonical == "engine") {
            items.push_back(makeCompletionItem("init", 3, target + ".init(w, h, title) -> null", "Initialize engine window with Vulkan PBR backend"));
            items.push_back(makeCompletionItem("set_fps", 3, target + ".set_fps(target) -> null", "Set target framerate cap"));
            items.push_back(makeCompletionItem("should_close", 3, target + ".should_close() -> bool", "Check whether window close is requested"));
            items.push_back(makeCompletionItem("get_delta_time", 3, target + ".get_delta_time() -> number", "Delta frame time in seconds"));
            items.push_back(makeCompletionItem("begin_draw", 3, target + ".begin_draw() -> null", "Begin frame draw buffer"));
            items.push_back(makeCompletionItem("end_draw", 3, target + ".end_draw() -> null", "End frame draw and swap buffers"));
            items.push_back(makeCompletionItem("clear", 3, target + ".clear(r, g, b) -> null", "Clear frame buffer with RGB color"));
            items.push_back(makeCompletionItem("draw_circle", 3, target + ".draw_circle(cx, cy, r, color) -> null", "Draw filled 2D circle"));
            items.push_back(makeCompletionItem("draw_circle_lines", 3, target + ".draw_circle_lines(cx, cy, r, color) -> null", "Draw 2D circle outline"));
            items.push_back(makeCompletionItem("draw_rect", 3, target + ".draw_rect(x, y, w, h, color) -> null", "Draw filled 2D rectangle"));
            items.push_back(makeCompletionItem("draw_rect_lines", 3, target + ".draw_rect_lines(x, y, w, h, color) -> null", "Draw 2D rectangle outline"));
            items.push_back(makeCompletionItem("draw_line", 3, target + ".draw_line(x1, y1, x2, y2, color) -> null", "Draw 2D line segment"));
            items.push_back(makeCompletionItem("draw_pixel", 3, target + ".draw_pixel(x, y, color) -> null", "Draw single pixel"));
            items.push_back(makeCompletionItem("draw_text", 3, target + ".draw_text(text, x, y, size, color) -> null", "Draw 2D text label"));
            items.push_back(makeCompletionItem("load_texture", 3, target + ".load_texture(path) -> texture", "Load 2D texture from image file"));
            items.push_back(makeCompletionItem("draw_texture", 3, target + ".draw_texture(tex, x, y) -> null", "Draw 2D texture at coordinates"));
            items.push_back(makeCompletionItem("unload_texture", 3, target + ".unload_texture(tex) -> null", "Unload texture from GPU memory"));
            items.push_back(makeCompletionItem("load_sound", 3, target + ".load_sound(path) -> sound", "Load audio file from disk"));
            items.push_back(makeCompletionItem("play_sound", 3, target + ".play_sound(sound) -> null", "Play loaded audio sound"));
            items.push_back(makeCompletionItem("stop_sound", 3, target + ".stop_sound(sound) -> null", "Stop audio sound playback"));
            items.push_back(makeCompletionItem("unload_sound", 3, target + ".unload_sound(sound) -> null", "Unload sound from audio device"));
            items.push_back(makeCompletionItem("set_sound_volume", 3, target + ".set_sound_volume(sound, volume) -> null", "Set audio volume 0.0 - 1.0"));
            items.push_back(makeCompletionItem("is_key_down", 3, target + ".is_key_down(key) -> bool", "Check if keyboard key is held down"));
            items.push_back(makeCompletionItem("is_key_pressed", 3, target + ".is_key_pressed(key) -> bool", "Check if keyboard key was pressed this frame"));
            items.push_back(makeCompletionItem("is_mouse_button_down", 3, target + ".is_mouse_button_down(btn) -> bool", "Check if mouse button is held down"));
            items.push_back(makeCompletionItem("is_mouse_button_pressed", 3, target + ".is_mouse_button_pressed(btn) -> bool", "Check if mouse button was clicked this frame"));
            items.push_back(makeCompletionItem("get_mouse_x", 3, target + ".get_mouse_x() -> number", "Get mouse X cursor coordinate"));
            items.push_back(makeCompletionItem("get_mouse_y", 3, target + ".get_mouse_y() -> number", "Get mouse Y cursor coordinate"));
            items.push_back(makeCompletionItem("get_mouse_delta_x", 3, target + ".get_mouse_delta_x() -> number", "Get mouse delta X movement"));
            items.push_back(makeCompletionItem("get_mouse_delta_y", 3, target + ".get_mouse_delta_y() -> number", "Get mouse delta Y movement"));
            items.push_back(makeCompletionItem("set_mouse_locked", 3, target + ".set_mouse_locked(locked) -> null", "Lock or release mouse cursor"));
            items.push_back(makeCompletionItem("set_pbr_environment", 3, target + ".set_pbr_environment(roughness, metallic, exposure) -> null", "Configure Vulkan Cook-Torrance GGX PBR environment"));
            items.push_back(makeCompletionItem("add_point_light", 3, target + ".add_point_light(x, y, z, r, g, b, intensity) -> null", "Add 3D point light to Vulkan scene"));
            items.push_back(makeCompletionItem("clear_lights", 3, target + ".clear_lights() -> null", "Clear all 3D scene point lights"));
            items.push_back(makeCompletionItem("set_camera", 3, target + ".set_camera(px, py, pz, tx, ty, tz, fov) -> null", "Configure 3D perspective camera position and target"));
            items.push_back(makeCompletionItem("draw_cube", 3, target + ".draw_cube(x, y, z, w, h, d, color) -> null", "Draw 3D solid cube"));
            items.push_back(makeCompletionItem("draw_cube_wires", 3, target + ".draw_cube_wires(x, y, z, w, h, d, color) -> null", "Draw 3D wireframe cube"));
            items.push_back(makeCompletionItem("draw_sphere", 3, target + ".draw_sphere(x, y, z, radius, color) -> null", "Draw 3D solid sphere"));
            items.push_back(makeCompletionItem("draw_sphere_wires", 3, target + ".draw_sphere_wires(x, y, z, radius, color) -> null", "Draw 3D wireframe sphere"));
            items.push_back(makeCompletionItem("draw_cylinder", 3, target + ".draw_cylinder(x, y, z, radius, height, color) -> null", "Draw 3D solid cylinder"));
            items.push_back(makeCompletionItem("draw_cone", 3, target + ".draw_cone(x, y, z, radius, height, color) -> null", "Draw 3D solid cone"));
            items.push_back(makeCompletionItem("draw_capsule", 3, target + ".draw_capsule(x, y, z, radius, height, color) -> null", "Draw 3D solid capsule"));
            items.push_back(makeCompletionItem("draw_line_3d", 3, target + ".draw_line_3d(x1, y1, z1, x2, y2, z2, color) -> null", "Draw 3D line segment"));
            items.push_back(makeCompletionItem("draw_ray", 3, target + ".draw_ray(ox, oy, oz, dx, dy, dz, length, color) -> null", "Draw 3D ray"));
            items.push_back(makeCompletionItem("draw_grid", 3, target + ".draw_grid(slices, spacing) -> null", "Draw 3D reference ground grid"));
            items.push_back(makeCompletionItem("draw_axes", 3, target + ".draw_axes(length) -> null", "Draw 3D XYZ coordinate axes"));
            items.push_back(makeCompletionItem("create_entity", 3, target + ".create_entity(tag) -> number", "Create scene entity and return ID"));
            items.push_back(makeCompletionItem("set_entity_position", 3, target + ".set_entity_position(id, x, y, z) -> null", "Set entity 3D position"));
            items.push_back(makeCompletionItem("set_entity_rotation", 3, target + ".set_entity_rotation(id, rx, ry, rz) -> null", "Set entity 3D rotation euler angles"));
            items.push_back(makeCompletionItem("set_entity_scale", 3, target + ".set_entity_scale(id, sx, sy, sz) -> null", "Set entity 3D scale vector"));
            items.push_back(makeCompletionItem("get_entity_position", 3, target + ".get_entity_position(id) -> object", "Get entity 3D position vector"));
            items.push_back(makeCompletionItem("get_entity_rotation", 3, target + ".get_entity_rotation(id) -> object", "Get entity 3D rotation vector"));
            items.push_back(makeCompletionItem("get_entity_scale", 3, target + ".get_entity_scale(id) -> object", "Get entity 3D scale vector"));
            items.push_back(makeCompletionItem("get_entity_tag", 3, target + ".get_entity_tag(id) -> string", "Get entity string tag"));
            items.push_back(makeCompletionItem("destroy_entity", 3, target + ".destroy_entity(id) -> null", "Destroy entity by ID"));
            items.push_back(makeCompletionItem("raycast", 3, target + ".raycast(ox, oy, oz, dx, dy, dz) -> object", "Perform 3D raycast against scene entities"));
            items.push_back(makeCompletionItem("get_fps", 3, target + ".get_fps() -> number", "Get current render framerate"));
            items.push_back(makeCompletionItem("get_draw_calls", 3, target + ".get_draw_calls() -> number", "Get GPU draw call count for frame"));
            items.push_back(makeCompletionItem("get_triangle_count", 3, target + ".get_triangle_count() -> number", "Get rendered triangle count"));
            items.push_back(makeCompletionItem("screenshot", 3, target + ".screenshot(filename) -> bool", "Save frame screenshot to file"));
            items.push_back(makeCompletionItem("close", 3, target + ".close() -> null", "Close engine window and free GPU resources"));
            return items;
        }

        if (target == "math") {
            items.push_back(makeCompletionItem("sqrt", 3, "math.sqrt(x) -> number", "Square root"));
            items.push_back(makeCompletionItem("abs", 3, "math.abs(x) -> number", "Absolute value"));
            items.push_back(makeCompletionItem("pow", 3, "math.pow(x, y) -> number", "Power"));
            items.push_back(makeCompletionItem("sin", 3, "math.sin(rad) -> number", "Sine"));
            items.push_back(makeCompletionItem("cos", 3, "math.cos(rad) -> number", "Cosine"));
            items.push_back(makeCompletionItem("tan", 3, "math.tan(rad) -> number", "Tangent"));
            items.push_back(makeCompletionItem("log", 3, "math.log(x) -> number", "Natural logarithm"));
            items.push_back(makeCompletionItem("floor", 3, "math.floor(x) -> number", "Floor"));
            items.push_back(makeCompletionItem("ceil", 3, "math.ceil(x) -> number", "Ceiling"));
            items.push_back(makeCompletionItem("round", 3, "math.round(x) -> number", "Round"));
            items.push_back(makeCompletionItem("pi", 21, "math.pi = 3.14159...", "Pi constant"));
            return items;
        }

        if (target == "json") {
            items.push_back(makeCompletionItem("parse", 3, "json.parse(str) -> value", "Parse JSON"));
            items.push_back(makeCompletionItem("stringify", 3, "json.stringify(val) -> string", "Serialize to JSON"));
            return items;
        }

        if (target == "fs") {
            items.push_back(makeCompletionItem("read", 3, "fs.read(path) -> string", "Read text file"));
            items.push_back(makeCompletionItem("write", 3, "fs.write(path, text) -> null", "Write text file"));
            items.push_back(makeCompletionItem("exists", 3, "fs.exists(path) -> bool", "Check if file exists"));
            items.push_back(makeCompletionItem("remove", 3, "fs.remove(path) -> null", "Remove file"));
            items.push_back(makeCompletionItem("mkdir", 3, "fs.mkdir(path) -> null", "Create directory"));
            return items;
        }

        if (target == "ffi") {
            items.push_back(makeCompletionItem("load", 3, "ffi.load(path) -> object", "Load dynamic library (.dll / .so / .dylib)"));
            items.push_back(makeCompletionItem("call", 3, "ffi.call(lib, fnName, ...args) -> value", "Call C symbol with automatic marshalling"));
            items.push_back(makeCompletionItem("sym", 3, "ffi.sym(lib, fnName) -> bool", "Check if symbol exists in library"));
            items.push_back(makeCompletionItem("close", 3, "ffi.close(lib) -> bool", "Unload dynamic library handle"));
            items.push_back(makeCompletionItem("callback", 3, "ffi.callback(fn, arity) -> number", "Create native C function pointer from closure"));
            return items;
        }

        if (target == "buffer") {
            items.push_back(makeCompletionItem("alloc", 3, "buffer.alloc(size) -> object", "Allocate zeroed byte buffer"));
            items.push_back(makeCompletionItem("from_string", 3, "buffer.from_string(str) -> object", "Create buffer from UTF-8 string"));
            items.push_back(makeCompletionItem("to_string", 3, "buffer.to_string(buf) -> string", "Convert buffer to string"));
            items.push_back(makeCompletionItem("size", 3, "buffer.size(buf) -> number", "Buffer size in bytes"));
            items.push_back(makeCompletionItem("slice", 3, "buffer.slice(buf, start, [end]) -> object", "Create sub-buffer slice"));
            items.push_back(makeCompletionItem("get_u8", 3, "buffer.get_u8(buf, offset) -> number", "Read unsigned 8-bit byte"));
            items.push_back(makeCompletionItem("set_u8", 3, "buffer.set_u8(buf, offset, val)", "Write unsigned 8-bit byte"));
            items.push_back(makeCompletionItem("get_i32", 3, "buffer.get_i32(buf, offset) -> number", "Read signed 32-bit int"));
            items.push_back(makeCompletionItem("set_i32", 3, "buffer.set_i32(buf, offset, val)", "Write signed 32-bit int"));
            items.push_back(makeCompletionItem("get_f64", 3, "buffer.get_f64(buf, offset) -> number", "Read 64-bit float"));
            items.push_back(makeCompletionItem("set_f64", 3, "buffer.set_f64(buf, offset, val)", "Write 64-bit float"));
            return items;
        }

        if (target == "struct") {
            items.push_back(makeCompletionItem("pack", 3, "struct.pack(fmt, ...vals) -> Buffer", "Pack values into binary Buffer according to format string"));
            items.push_back(makeCompletionItem("unpack", 3, "struct.unpack(fmt, buf, [offset]) -> array", "Unpack binary Buffer according to format string"));
            items.push_back(makeCompletionItem("calcsize", 3, "struct.calcsize(fmt) -> number", "Compute byte size of binary format string"));
            return items;
        }

        if (target == "crypto") {
            items.push_back(makeCompletionItem("sha256", 3, "crypto.sha256(str) -> string", "Compute SHA-256 hex digest"));
            items.push_back(makeCompletionItem("sha1", 3, "crypto.sha1(str) -> string", "Compute SHA-1 hex digest"));
            items.push_back(makeCompletionItem("md5", 3, "crypto.md5(str) -> string", "Compute MD5 hex digest"));
            items.push_back(makeCompletionItem("base64_encode", 3, "crypto.base64_encode(str) -> string", "Encode string to Base64"));
            items.push_back(makeCompletionItem("base64_decode", 3, "crypto.base64_decode(b64) -> string", "Decode Base64 string"));
            items.push_back(makeCompletionItem("uuid", 3, "crypto.uuid() -> string", "Generate UUID v4"));
            items.push_back(makeCompletionItem("hmac_sha256", 3, "crypto.hmac_sha256(msg, key) -> string", "Compute HMAC-SHA256 hex digest"));
            items.push_back(makeCompletionItem("random_bytes", 3, "crypto.random_bytes(n) -> string", "Generate random hex bytes"));
            return items;
        }

        if (target == "process") {
            items.push_back(makeCompletionItem("cwd", 3, "process.cwd() -> string", "Current working directory"));
            items.push_back(makeCompletionItem("pid", 3, "process.pid() -> number", "Current process ID"));
            items.push_back(makeCompletionItem("args", 3, "process.args() -> array", "Command-line arguments"));
            items.push_back(makeCompletionItem("exit", 3, "process.exit([code]) -> null", "Terminate process with exit code"));
            return items;
        }

        if (target == "env") {
            items.push_back(makeCompletionItem("get", 3, "env.get(name, [default]) -> string", "Get environment variable"));
            items.push_back(makeCompletionItem("set", 3, "env.set(name, value) -> bool", "Set environment variable"));
            items.push_back(makeCompletionItem("has", 3, "env.has(name) -> bool", "Check if environment variable exists"));
            items.push_back(makeCompletionItem("all", 3, "env.all() -> object", "Get all environment variables"));
            return items;
        }

        if (target == "os") {
            items.push_back(makeCompletionItem("platform", 3, "os.platform() -> string", "Operating system name"));
            items.push_back(makeCompletionItem("arch", 3, "os.arch() -> string", "CPU architecture"));
            items.push_back(makeCompletionItem("cpus", 3, "os.cpus() -> number", "Hardware concurrency count"));
            items.push_back(makeCompletionItem("exec", 3, "os.exec(cmd) -> string", "Execute shell command and capture output"));
            return items;
        }

        if (target == "datetime") {
            items.push_back(makeCompletionItem("now", 3, "datetime.now() -> object", "Current datetime"));
            items.push_back(makeCompletionItem("timestamp", 3, "datetime.timestamp() -> number", "UNIX timestamp"));
            items.push_back(makeCompletionItem("format", 3, "datetime.format(ts, pat) -> string", "Format timestamp"));
            return items;
        }

        if (target == "path") {
            items.push_back(makeCompletionItem("join", 3, "path.join(...parts) -> string", "Join path segments"));
            items.push_back(makeCompletionItem("basename", 3, "path.basename(p) -> string", "Filename component"));
            items.push_back(makeCompletionItem("dirname", 3, "path.dirname(p) -> string", "Directory path"));
            items.push_back(makeCompletionItem("ext", 3, "path.ext(p) -> string", "File extension"));
            items.push_back(makeCompletionItem("stem", 3, "path.stem(p) -> string", "File stem"));
            items.push_back(makeCompletionItem("exists", 3, "path.exists(p) -> bool", "Check path exists"));
            items.push_back(makeCompletionItem("abs", 3, "path.abs(p) -> string", "Absolute path"));
            return items;
        }

        if (target == "random") {
            items.push_back(makeCompletionItem("random", 3, "random.random() -> number", "Random float [0, 1)"));
            items.push_back(makeCompletionItem("int", 3, "random.int(min, max) -> number", "Random integer"));
            items.push_back(makeCompletionItem("choice", 3, "random.choice(arr) -> value", "Random choice"));
            items.push_back(makeCompletionItem("shuffle", 3, "random.shuffle(arr) -> array", "Shuffle array"));
            return items;
        }

        std::regex structRegex(R"(struct\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{([^}]*)\})");
        textBegin = docText.cbegin();
        while (std::regex_search(textBegin, docText.cend(), match, structRegex)) {
            std::string structName = match[1].str();
            std::string body = match[2].str();
            std::regex fieldRegex(R"(([A-Za-z_][A-Za-z0-9_]*)\s*:)");
            std::smatch fMatch;
            auto fBegin = body.cbegin();
            while (std::regex_search(fBegin, body.cend(), fMatch, fieldRegex)) {
                items.push_back(makeCompletionItem(fMatch[1].str(), 5, structName + " field"));
                fBegin = fMatch.suffix().first;
            }
            textBegin = match.suffix().first;
        }

        return items;
    }

    dap::json handleCompletion(const std::string& uri, int line, int character) {
        dap::json items = dap::json::array();
        auto it = documents_.find(uri);
        if (it == documents_.end()) return items;

        std::string prefix = getLinePrefix(it->second, line, character);

        size_t importQuote = prefix.find("import \"");
        if (importQuote == std::string::npos) importQuote = prefix.find("import '");
        if (importQuote != std::string::npos) {
            std::string entered = prefix.substr(importQuote + 8);
            std::filesystem::path docPath = uriToPath(uri);
            std::filesystem::path dir = docPath.parent_path();
            if (entered == "./" || entered == ".\\") {
                dir = docPath.parent_path();
            } else if (entered.find('/') != std::string::npos || entered.find('\\') != std::string::npos) {
                std::filesystem::path sub = entered;
                dir = (docPath.parent_path() / sub).parent_path();
            }
            std::error_code ec;
            if (std::filesystem::exists(dir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory(ec)) {
                        items.push_back(makeCompletionItem(name + "/", 17, "folder"));
                    } else if (entry.path().extension() == ".rq" || entry.path().extension() == ".rqio") {
                        items.push_back(makeCompletionItem(name, 17, "RayQuiro source file"));
                    }
                }
            }
            items.push_back(makeCompletionItem("rayquiro.engine", 9, "Built-in 2D/3D Game Engine module"));
            items.push_back(makeCompletionItem("rayquiro.app", 9, "Built-in Desktop Application Shell"));
            items.push_back(makeCompletionItem("rayquiro.ui", 9, "Built-in UI Component Framework"));
            items.push_back(makeCompletionItem("rayquiro.web", 9, "Built-in Web Server and HTML module"));
            items.push_back(makeCompletionItem("rayquiro.random", 9, "Built-in Random and Probability module"));
            items.push_back(makeCompletionItem("rayquiro.math", 9, "Built-in Mathematical Functions module"));
            items.push_back(makeCompletionItem("rayquiro.datetime", 9, "Built-in Date and Time module"));
            items.push_back(makeCompletionItem("rayquiro.path", 9, "Built-in Filesystem Path module"));
            return items;
        }

        size_t dotPos = prefix.rfind('.');
        if (dotPos != std::string::npos && (dotPos + 1 == prefix.size() || std::all_of(prefix.begin() + dotPos + 1, prefix.end(), [](char c){ return std::isspace((unsigned char)c); }))) {
            size_t idEnd = dotPos;
            while (idEnd > 0 && std::isspace((unsigned char)prefix[idEnd - 1])) idEnd--;
            size_t idStart = idEnd;
            while (idStart > 0 && (std::isalnum((unsigned char)prefix[idStart - 1]) || prefix[idStart - 1] == '_')) {
                idStart--;
            }
            std::string target = prefix.substr(idStart, idEnd - idStart);
            if (!target.empty()) {
                return resolveMemberCompletions(uri, target);
            }
        }

        std::vector<std::string> keywords = {
            "fn", "var", "let", "const", "spawn", "struct", "enum", "interface", "impl",
            "try", "catch", "finally", "throw", "if", "else", "while", "for", "in",
            "break", "continue", "return", "import", "from", "export", "as", "null", "true", "false",
            "switch", "case", "default", "match", "defer", "test", "async", "await"
        };
        for (const auto& kw : keywords) {
            items.push_back(makeCompletionItem(kw, 14, "keyword"));
        }

        std::vector<std::string> types = {
            "int", "float", "number", "string", "bool", "void", "any"
        };
        for (const auto& t : types) {
            items.push_back(makeCompletionItem(t, 25, "Type"));
        }

        std::vector<std::pair<std::string, std::string>> globals = {
            {"print", "print(val) -> null"},
            {"len", "len(val) -> number"},
            {"slice", "slice(target, start, [end]) -> value"},
            {"str", "str(val) -> string"},
            {"num", "num(val) -> number"},
            {"bool", "bool(val) -> bool"},
            {"type", "type(val) -> string"},
            {"assert", "assert(cond, [msg]) -> bool"},
            {"assert_eq", "assert_eq(a, b, [msg]) -> bool"},
            {"assert_ne", "assert_ne(a, b, [msg]) -> bool"},
            {"channel", "channel() -> Channel"},
            {"channel_send", "channel_send(ch, val)"},
            {"channel_recv", "channel_recv(ch) -> value"},
            {"channel_close", "channel_close(ch)"},
            {"vec2", "vec2(x, y) -> {x, y}"},
            {"vec3", "vec3(x, y, z) -> {x, y, z}"},
            {"vec_add", "vec_add(a, b) -> vec"},
            {"vec_sub", "vec_sub(a, b) -> vec"},
            {"vec_mul", "vec_mul(v, s) -> vec"},
            {"vec_dot", "vec_dot(a, b) -> number"},
            {"vec_len", "vec_len(v) -> number"},
            {"vec_normalize", "vec_normalize(v) -> vec"},
            {"vec_dist", "vec_dist(a, b) -> number"},
            {"vec_lerp", "vec_lerp(a, b, t) -> vec"},
            {"vec_cross", "vec_cross(a, b) -> vec3"},
            {"sqrt", "sqrt(x) -> number"},
            {"abs", "abs(x) -> number"},
            {"pow", "pow(x, y) -> number"},
            {"pi", "pi = 3.14159..."}
        };
        for (const auto& [name, sig] : globals) {
            items.push_back(makeCompletionItem(name, 3, sig));
        }

        {
            std::unordered_map<std::string, int> userSyms;
            const std::string& docText = it->second;
            size_t pos = 0;
            while (pos < docText.size()) {
                if (pos + 3 <= docText.size() && docText.substr(pos, 3) == "fn ") {
                    pos += 3;
                    while (pos < docText.size() && std::isspace(static_cast<unsigned char>(docText[pos]))) pos++;
                    size_t idStart = pos;
                    while (pos < docText.size() && (std::isalnum(static_cast<unsigned char>(docText[pos])) || docText[pos] == '_')) pos++;
                    if (pos > idStart) {
                        std::string sym = docText.substr(idStart, pos - idStart);
                        userSyms[sym] = 3;
                    }
                } else if (pos + 6 <= docText.size() && docText.substr(pos, 6) == "const ") {
                    pos += 6;
                    while (pos < docText.size() && std::isspace(static_cast<unsigned char>(docText[pos]))) pos++;
                    size_t idStart = pos;
                    while (pos < docText.size() && (std::isalnum(static_cast<unsigned char>(docText[pos])) || docText[pos] == '_')) pos++;
                    if (pos > idStart) {
                        std::string sym = docText.substr(idStart, pos - idStart);
                        userSyms[sym] = 21;
                    }
                } else if (pos + 4 <= docText.size() && (docText.substr(pos, 4) == "var " || docText.substr(pos, 4) == "let ")) {
                    pos += 4;
                    while (pos < docText.size() && std::isspace(static_cast<unsigned char>(docText[pos]))) pos++;
                    size_t idStart = pos;
                    while (pos < docText.size() && (std::isalnum(static_cast<unsigned char>(docText[pos])) || docText[pos] == '_')) pos++;
                    if (pos > idStart) {
                        std::string sym = docText.substr(idStart, pos - idStart);
                        userSyms[sym] = 6;
                    }
                } else if (pos + 7 <= docText.size() && docText.substr(pos, 7) == "struct ") {
                    pos += 7;
                    while (pos < docText.size() && std::isspace(static_cast<unsigned char>(docText[pos]))) pos++;
                    size_t idStart = pos;
                    while (pos < docText.size() && (std::isalnum(static_cast<unsigned char>(docText[pos])) || docText[pos] == '_')) pos++;
                    if (pos > idStart) {
                        std::string sym = docText.substr(idStart, pos - idStart);
                        userSyms[sym] = 22;
                    }
                } else if (pos + 5 <= docText.size() && docText.substr(pos, 5) == "enum ") {
                    pos += 5;
                    while (pos < docText.size() && std::isspace(static_cast<unsigned char>(docText[pos]))) pos++;
                    size_t idStart = pos;
                    while (pos < docText.size() && (std::isalnum(static_cast<unsigned char>(docText[pos])) || docText[pos] == '_')) pos++;
                    if (pos > idStart) {
                        std::string sym = docText.substr(idStart, pos - idStart);
                        userSyms[sym] = 13;
                    }
                } else {
                    pos++;
                }
            }
            for (const auto& pair : userSyms) {
                std::string desc = (pair.second == 3) ? "User function" :
                                   ((pair.second == 22) ? "User struct" :
                                   ((pair.second == 13) ? "User enum" :
                                   ((pair.second == 21) ? "User constant" : "User variable")));
                items.push_back(makeCompletionItem(pair.first, pair.second, desc));
            }
        }

        std::vector<std::string> namespaces = {
            "math", "random", "web", "json", "fs", "datetime", "path", "env", "process", "os", "crypto", "engine", "ffi", "buffer", "struct"
        };
        for (const auto& ns : namespaces) {
            items.push_back(makeCompletionItem(ns, 9, "Built-in namespace"));
        }

        return items;
    }

    dap::json handleHover(const std::string& uri, int line, int character) {
        auto it = documents_.find(uri);
        if (it == documents_.end()) return dap::json();

        std::string word = getWordAtPosition(it->second, line, character);
        if (word.empty()) return dap::json();

        std::string doc;
        if (word == "fn") doc = "**fn** keyword\n\nDeclares a named function or closure with optional default parameters and variadic `...rest`.\n\n```rq\nfn add(a, b = 10) {\n    return a + b;\n}\n```";
        else if (word == "struct") doc = "**struct** keyword / binary packing namespace\n\nDefines a structured value type with default field initializers, or packs/unpacks binary buffers (`struct.pack`, `struct.unpack`, `struct.calcsize`).\n\n```rq\nstruct Point {\n    x: 0,\n    y: 0\n}\n```";
        else if (word == "defer") doc = "**defer** statement\n\nSchedules a statement or block to execute in LIFO order when the enclosing scope exits (including early `return` or `throw`).\n\n```rq\ndefer ffi.close(lib);\n```";
        else if (word == "match") doc = "**match** statement\n\nPattern matching over literals, inclusive ranges (`a..b`), multiple alternatives (`|`), wildcard `_`, and `if` guards.\n\n```rq\nmatch (code) {\n    200..299 => print(\"OK\"),\n    x if code == 404 => print(\"Not Found\"),\n    _ => print(\"Error\")\n}\n```";
        else if (word == "test") doc = "**test** block\n\nDeclares a native unit test executed via `rqio test`.\n\n```rq\ntest \"math check\" {\n    assert_eq(2 + 2, 4);\n}\n```";
        else if (word == "spawn") doc = "**spawn** statement\n\nSpawns a concurrent worker thread executing the given call or block.\n\n```rq\nspawn worker(ch, 1);\n```";
        else if (word == "channel") doc = "**channel**() -> Channel\n\nCreates a thread-safe CSP channel supporting `.send(val)`, `.recv()`, and `.close()`.";
        else if (word == "buffer" || word == "buffer.alloc") doc = "**buffer** namespace\n\nRaw memory buffer operations (`alloc`, `from_string`, `to_string`, `size`, `slice`, `get_u8`, `set_u8`, `get_i32`, `set_i32`, `get_f64`, `set_f64`).";
        else if (word == "ffi" || word == "ffi.load" || word == "ffi.call") doc = "**ffi** namespace\n\nZero-overhead Foreign Function Interface (`ffi.load`, `ffi.call`, `ffi.sym`, `ffi.callback`, `ffi.close`).";
        else if (word == "var" || word == "let") doc = "**var / let** keyword\n\nDeclares a mutable variable in the current scope with optional array or object destructuring.\n\n```rq\nvar [first, second] = list;\nvar {name, version} = info;\n```";
        else if (word == "try" || word == "catch" || word == "finally") doc = "**try-catch-finally** block\n\nHandles runtime exceptions and errors thrown by `throw`.\n\n```rq\ntry {\n    throw \"error\";\n} catch (e) {\n    print(e);\n} finally {\n    print(\"cleanup\");\n}\n```";
        else if (word == "import" || word == "export") doc = "**import / export** module system\n\nImports or exports symbols across `.rq` modules and built-in namespaces.\n\n```rq\nimport { sum, PI } from \"./math.rq\";\nexport fn calculate(x) { return x * 2; }\n```";
        else if (word == "print") doc = "**print**(value) -> null\n\nPrints a string representation of the given value to standard output.";
        else if (word == "len") doc = "**len**(val) -> number\n\nReturns the length of a string, array, buffer, or number of keys in an object.";
        else if (word == "slice") doc = "**slice**(target, start, [end]) -> value\n\nExtracts a slice from an array, string, or raw memory buffer.";
        else if (word == "assert" || word == "assert_eq" || word == "assert_ne") doc = "**assert / assert_eq / assert_ne**\n\nBuilt-in runtime assertions that throw an error when the condition fails.";
        else if (word == "sqrt") doc = "**sqrt**(x) -> number\n\nReturns the square root of number `x`.";
        else if (word == "pow") doc = "**pow**(base, exp) -> number\n\nReturns `base` raised to the power of `exp`.";
        else if (word == "abs") doc = "**abs**(x) -> number\n\nReturns the absolute value of `x`.";
        else if (word == "math.pi" || word == "pi") doc = "**pi**: 3.141592653589793\n\nMathematical constant representing the ratio of circumference to diameter.";
        else if (word == "json.parse") doc = "**json.parse**(json_str) -> value\n\nParses a JSON string into a RayQuiro table/array/primitive.";
        else if (word == "json.stringify") doc = "**json.stringify**(value) -> string\n\nSerializes a RayQuiro value into a formatted JSON string.";
        else if (word == "fs.read") doc = "**fs.read**(path) -> string\n\nReads and returns the complete text contents of a file.";
        else if (word == "fs.write") doc = "**fs.write**(path, text) -> null\n\nWrites text contents to a file, creating it if it does not exist.";
        else if (word == "datetime.now") doc = "**datetime.now**() -> object\n\nReturns current local date and time object containing year, month, day, hour, minute, second.";

        if (doc.empty()) return dap::json();

        dap::json result;
        dap::json contents;
        contents["kind"] = "markdown";
        contents["value"] = doc;
        result["contents"] = contents;
        return result;
    }

public:
    int run() {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        while (running_) {
            std::string raw = readMessage();
            if (raw.empty()) {
                break;
            }
            dap::json msg = dap::json::parse(raw);
            if (!msg.is_object()) continue;

            std::string method = msg.value("method", std::string(""));
            dap::json id = msg.contains("id") ? msg["id"] : dap::json();

            if (method == "initialize") {
                dap::json result;
                dap::json capabilities;
                capabilities["textDocumentSync"] = 1;
                dap::json completion;
                completion["resolveProvider"] = false;
                dap::json triggers = dap::json::array();
                triggers.push_back(".");
                triggers.push_back("\"");
                triggers.push_back("'");
                triggers.push_back("/");
                triggers.push_back(":");
                completion["triggerCharacters"] = triggers;
                capabilities["completionProvider"] = completion;
                capabilities["hoverProvider"] = true;
                result["capabilities"] = capabilities;

                dap::json serverInfo;
                serverInfo["name"] = "rayquiro-lsp";
                serverInfo["version"] = "1.0";
                result["serverInfo"] = serverInfo;

                sendResponse(id, result);
            } else if (method == "initialized") {
            } else if (method == "shutdown") {
                sendResponse(id, dap::json());
            } else if (method == "exit") {
                running_ = false;
                break;
            } else if (method == "textDocument/didOpen") {
                if (msg.contains("params") && msg["params"].contains("textDocument")) {
                    const auto& doc = msg["params"]["textDocument"];
                    std::string uri = doc.value("uri", std::string(""));
                    std::string text = doc.value("text", std::string(""));
                    documents_[uri] = text;
                    validateDocument(uri);
                }
            } else if (method == "textDocument/didChange") {
                if (msg.contains("params") && msg["params"].contains("textDocument")) {
                    const auto& doc = msg["params"]["textDocument"];
                    std::string uri = doc.value("uri", std::string(""));
                    if (msg["params"].contains("contentChanges") && msg["params"]["contentChanges"].size() > 0) {
                        std::string text = msg["params"]["contentChanges"][0].value("text", std::string(""));
                        documents_[uri] = text;
                        validateDocument(uri);
                    }
                }
            } else if (method == "textDocument/didClose") {
                if (msg.contains("params") && msg["params"].contains("textDocument")) {
                    std::string uri = msg["params"]["textDocument"].value("uri", std::string(""));
                    documents_.erase(uri);
                    dap::json p;
                    p["uri"] = uri;
                    p["diagnostics"] = dap::json::array();
                    sendNotification("textDocument/publishDiagnostics", p);
                }
            } else if (method == "textDocument/completion") {
                if (msg.contains("params")) {
                    std::string uri = msg["params"]["textDocument"].value("uri", std::string(""));
                    int line = msg["params"]["position"].value("line", 0);
                    int character = msg["params"]["position"].value("character", 0);
                    sendResponse(id, handleCompletion(uri, line, character));
                } else {
                    sendResponse(id, dap::json::array());
                }
            } else if (method == "textDocument/hover") {
                if (msg.contains("params")) {
                    std::string uri = msg["params"]["textDocument"].value("uri", std::string(""));
                    int line = msg["params"]["position"].value("line", 0);
                    int character = msg["params"]["position"].value("character", 0);
                    sendResponse(id, handleHover(uri, line, character));
                } else {
                    sendResponse(id, dap::json());
                }
            } else if (!id.is_null()) {
                sendResponse(id, dap::json());
            }
        }
        return 0;
    }
};
