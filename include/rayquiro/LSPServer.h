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

            dap::json diag;
            dap::json range;
            dap::json startPos;
            startPos["line"] = errLine;
            startPos["character"] = errCol;
            dap::json endPos;
            endPos["line"] = errLine;
            endPos["character"] = errCol + 1;
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

        std::string importedModule;
        std::regex importAsRegex(R"(import\s+["']([^"']+)["']\s+as\s+([A-Za-z_][A-Za-z0-9_]*))");
        std::smatch match;
        auto textBegin = docText.cbegin();
        while (std::regex_search(textBegin, docText.cend(), match, importAsRegex)) {
            if (match[2].str() == target) {
                importedModule = match[1].str();
                break;
            }
            textBegin = match.suffix().first;
        }

        if (importedModule.empty()) {
            std::regex importBareRegex(R"(import\s+["']([^"']+)["'])");
            textBegin = docText.cbegin();
            while (std::regex_search(textBegin, docText.cend(), match, importBareRegex)) {
                std::string full = match[1].str();
                std::string base = full;
                size_t dot = base.rfind('.');
                if (dot != std::string::npos) base = base.substr(dot + 1);
                if (base == target || full == target) {
                    importedModule = full;
                    break;
                }
                textBegin = match.suffix().first;
            }
        }

        if (importedModule.empty()) {
            importedModule = target;
        }

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

        if (importedModule == "telebot" || target == "telebot") {
            items.push_back(makeCompletionItem("init", 3, "telebot.init(token) -> Bot", "Initialize a Telegram bot client instance"));
            items.push_back(makeCompletionItem("send_message", 3, "telebot.send_message(chat_id, text) -> bool", "Send a text message to a chat"));
            items.push_back(makeCompletionItem("on_message", 3, "telebot.on_message(bot, fn) -> null", "Register a handler for incoming messages"));
            items.push_back(makeCompletionItem("command", 3, "telebot.command(bot, cmd, fn) -> null", "Register a command handler"));
            items.push_back(makeCompletionItem("polling", 3, "telebot.polling(bot) -> null", "Start long polling for updates"));
            items.push_back(makeCompletionItem("get_updates", 3, "telebot.get_updates(bot) -> array", "Fetch new pending updates"));
            items.push_back(makeCompletionItem("set_webhook", 3, "telebot.set_webhook(bot, url) -> bool", "Set custom Telegram webhook URL"));
            return items;
        }

        if (importedModule == "rayquiro.ui" || target == "ui") {
            items.push_back(makeCompletionItem("label", 3, "ui.label(text) -> UIElement", "Create a static text label"));
            items.push_back(makeCompletionItem("row", 3, "ui.row(children) -> UIElement", "Create horizontal flex container"));
            items.push_back(makeCompletionItem("column", 3, "ui.column(children) -> UIElement", "Create vertical stack container"));
            items.push_back(makeCompletionItem("textField", 3, "ui.textField(options) -> UIElement", "Create text input field with placeholder & onChange"));
            items.push_back(makeCompletionItem("text_field", 3, "ui.text_field(options) -> UIElement", "Create text input field"));
            items.push_back(makeCompletionItem("checkbox", 3, "ui.checkbox(label, checked, onChange) -> UIElement", "Create toggle checkbox"));
            items.push_back(makeCompletionItem("slider", 3, "ui.slider(min, max, val, onChange) -> UIElement", "Create numeric slider"));
            items.push_back(makeCompletionItem("button", 3, "ui.button(text, onClick) -> UIElement", "Create interactive action button"));
            items.push_back(makeCompletionItem("render", 3, "ui.render(root) -> null", "Render declarative component tree"));
            return items;
        }

        if (importedModule == "rayquiro.app" || target == "app") {
            items.push_back(makeCompletionItem("window", 3, "app.window(options) -> Window", "Create desktop application window"));
            items.push_back(makeCompletionItem("show_message", 3, "app.show_message(title, text) -> null", "Show native message box"));
            items.push_back(makeCompletionItem("init", 3, "app.init(w, h, title) -> null", "Initialize application shell"));
            items.push_back(makeCompletionItem("run", 3, "app.run() -> null", "Start application message loop"));
            items.push_back(makeCompletionItem("button", 3, "app.button(id, text, x, y, w, h) -> null", "Add native button control"));
            items.push_back(makeCompletionItem("text", 3, "app.text(id, text, x, y, w, h) -> null", "Add native static text control"));
            items.push_back(makeCompletionItem("msg", 3, "app.msg(title, text) -> null", "Display native message box"));
            return items;
        }

        if (importedModule == "rayquiro.web" || target == "web") {
            items.push_back(makeCompletionItem("server", 3, "web.server() -> object", "Create web server instance"));
            items.push_back(makeCompletionItem("page", 3, "web.page(title, body) -> null", "Emit HTML page"));
            items.push_back(makeCompletionItem("begin", 3, "web.begin(title) -> null", "Begin HTML site generation"));
            items.push_back(makeCompletionItem("route", 3, "web.route(path, title) -> null", "Define web route"));
            items.push_back(makeCompletionItem("head", 3, "web.head(html) -> null", "Append raw HTML to head"));
            items.push_back(makeCompletionItem("public", 3, "web.public(dir) -> null", "Set public static assets directory"));
            items.push_back(makeCompletionItem("live", 3, "web.live(port) -> null", "Start live dev server"));
            items.push_back(makeCompletionItem("style", 3, "web.style(css) -> null", "Apply global stylesheet"));
            items.push_back(makeCompletionItem("text", 3, "web.text(str) -> null", "Append text block"));
            items.push_back(makeCompletionItem("h1", 3, "web.h1(str) -> null", "Append heading 1"));
            items.push_back(makeCompletionItem("h2", 3, "web.h2(str) -> null", "Append heading 2"));
            items.push_back(makeCompletionItem("p", 3, "web.p(str) -> null", "Append paragraph"));
            items.push_back(makeCompletionItem("button", 3, "web.button(text) -> null", "Append HTML button"));
            items.push_back(makeCompletionItem("raw", 3, "web.raw(html) -> null", "Append raw HTML snippet"));
            items.push_back(makeCompletionItem("end", 3, "web.end() -> null", "Finalize web site build"));
            items.push_back(makeCompletionItem("get", 3, "web.get(url, [headers]) -> object", "HTTP GET request"));
            items.push_back(makeCompletionItem("post", 3, "web.post(url, body, [headers]) -> object", "HTTP POST request"));
            return items;
        }

        if (importedModule == "rayquiro.engine" || target == "engine") {
            items.push_back(makeCompletionItem("init", 3, "engine.init(w, h, title) -> null", "Initialize engine"));
            items.push_back(makeCompletionItem("set_fps", 3, "engine.set_fps(target) -> null", "Set target FPS"));
            items.push_back(makeCompletionItem("should_close", 3, "engine.should_close() -> bool", "Check close request"));
            items.push_back(makeCompletionItem("get_delta_time", 3, "engine.get_delta_time() -> number", "Delta frame time"));
            items.push_back(makeCompletionItem("begin_draw", 3, "engine.begin_draw() -> null", "Begin frame draw"));
            items.push_back(makeCompletionItem("end_draw", 3, "engine.end_draw() -> null", "End frame draw"));
            items.push_back(makeCompletionItem("clear", 3, "engine.clear(r, g, b) -> null", "Clear color"));
            items.push_back(makeCompletionItem("draw_circle", 3, "engine.draw_circle(cx, cy, r, color) -> null", "Draw circle"));
            items.push_back(makeCompletionItem("draw_rect", 3, "engine.draw_rect(x, y, w, h, color) -> null", "Draw rect"));
            items.push_back(makeCompletionItem("draw_text", 3, "engine.draw_text(text, x, y, size, color) -> null", "Draw text"));
            items.push_back(makeCompletionItem("load_texture", 3, "engine.load_texture(path) -> texture", "Load texture"));
            items.push_back(makeCompletionItem("draw_texture", 3, "engine.draw_texture(tex, x, y) -> null", "Draw texture"));
            items.push_back(makeCompletionItem("load_sound", 3, "engine.load_sound(path) -> sound", "Load sound"));
            items.push_back(makeCompletionItem("play_sound", 3, "engine.play_sound(sound) -> null", "Play sound"));
            items.push_back(makeCompletionItem("close", 3, "engine.close() -> null", "Close engine window"));
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
            "fn", "var", "let", "struct", "try", "catch", "finally",
            "throw", "if", "else", "while", "for", "in", "break",
            "continue", "return", "import", "as", "null", "true", "false"
        };
        for (const auto& kw : keywords) {
            items.push_back(makeCompletionItem(kw, 14, "keyword"));
        }

        std::vector<std::pair<std::string, std::string>> globals = {
            {"print", "print(val) -> null"},
            {"len", "len(val) -> number"},
            {"str", "str(val) -> string"},
            {"num", "num(val) -> number"},
            {"bool", "bool(val) -> bool"},
            {"type", "type(val) -> string"},
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
                } else {
                    pos++;
                }
            }
            for (const auto& pair : userSyms) {
                std::string desc = (pair.second == 3) ? "User function" : ((pair.second == 22) ? "User struct" : "User variable");
                items.push_back(makeCompletionItem(pair.first, pair.second, desc));
            }
        }

        std::vector<std::string> namespaces = {
            "math", "random", "web", "json", "fs", "datetime", "path", "env", "process", "crypto", "engine"
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
        if (word == "fn") doc = "**fn** keyword\n\nDeclares a named function or lambda expression with optional default parameters.\n\n```rq\nfn add(a, b = 10) {\n    return a + b;\n}\n```";
        else if (word == "struct") doc = "**struct** keyword\n\nDefines a user-defined structured value type with default field initializers.\n\n```rq\nstruct Point {\n    x: 0,\n    y: 0\n}\n```";
        else if (word == "var" || word == "let") doc = "**var / let** keyword\n\nDeclares a new variable in the current scope with optional destructuring.\n\n```rq\nvar [first, second] = list;\nvar {name, version} = info;\n```";
        else if (word == "try" || word == "catch" || word == "finally") doc = "**try-catch-finally** block\n\nHandles runtime exceptions and errors thrown by `throw`.\n\n```rq\ntry {\n    throw \"error\";\n} catch (e) {\n    print(e);\n} finally {\n    print(\"cleanup\");\n}\n```";
        else if (word == "import") doc = "**import** statement\n\nImports a built-in module, external package, or local `.rq` file.\n\n```rq\nimport \"rayquiro.engine\" as engine;\nimport \"./utils.rq\" as utils;\n```";
        else if (word == "print") doc = "**print**(value) -> null\n\nPrints a string representation of the given value to standard output.";
        else if (word == "len") doc = "**len**(val) -> number\n\nReturns the length of a string, array, or number of keys in an object.";
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
                serverInfo["version"] = "0.2.1";
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
