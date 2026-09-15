#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <algorithm>

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

    std::string uriToPath(const std::string& uri) {
        std::string prefix = "file://";
        if (uri.rfind(prefix, 0) == 0) {
            std::string p = uri.substr(prefix.size());
#ifdef _WIN32
            if (p.size() >= 3 && p[0] == '/' && std::isalpha(static_cast<unsigned char>(p[1])) && p[2] == ':') {
                p = p.substr(1);
            }
#endif
            return p;
        }
        return uri;
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
            if (entered.find('/') != std::string::npos || entered.find('\\') != std::string::npos) {
                std::filesystem::path sub = entered;
                dir = (dir / sub).parent_path();
            }
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory()) {
                        items.push_back(makeCompletionItem(name + "/", 17, "folder"));
                    } else if (entry.path().extension() == ".rq") {
                        items.push_back(makeCompletionItem(name, 17, "RayQuiro source file"));
                    }
                }
            }
            items.push_back(makeCompletionItem("rayquiro.engine", 9, "Built-in 2D/3D Game Engine module"));
            items.push_back(makeCompletionItem("rayquiro.app", 9, "Built-in Desktop Application Shell"));
            items.push_back(makeCompletionItem("rayquiro.ui", 9, "Built-in UI Component Framework"));
            items.push_back(makeCompletionItem("rayquiro.web", 9, "Built-in Web Server and HTML module"));
            return items;
        }

        if (prefix.size() >= 5 && prefix.substr(prefix.size() - 5) == "math.") {
            items.push_back(makeCompletionItem("sqrt", 3, "sqrt(x) -> number", "Square root of x"));
            items.push_back(makeCompletionItem("abs", 3, "abs(x) -> number", "Absolute value of x"));
            items.push_back(makeCompletionItem("pow", 3, "pow(base, exp) -> number", "Base raised to exponent"));
            items.push_back(makeCompletionItem("sin", 3, "sin(rad) -> number", "Sine of angle in radians"));
            items.push_back(makeCompletionItem("cos", 3, "cos(rad) -> number", "Cosine of angle in radians"));
            items.push_back(makeCompletionItem("tan", 3, "tan(rad) -> number", "Tangent of angle in radians"));
            items.push_back(makeCompletionItem("log", 3, "log(x) -> number", "Natural logarithm of x"));
            items.push_back(makeCompletionItem("floor", 3, "floor(x) -> number", "Largest integer <= x"));
            items.push_back(makeCompletionItem("ceil", 3, "ceil(x) -> number", "Smallest integer >= x"));
            items.push_back(makeCompletionItem("round", 3, "round(x) -> number", "Rounds x to nearest integer"));
            items.push_back(makeCompletionItem("pi", 21, "pi: 3.14159...", "Mathematical constant pi"));
            return items;
        }

        if (prefix.size() >= 5 && prefix.substr(prefix.size() - 5) == "json.") {
            items.push_back(makeCompletionItem("parse", 3, "json.parse(str) -> value", "Parses JSON string into RayQuiro object/array"));
            items.push_back(makeCompletionItem("stringify", 3, "json.stringify(val) -> string", "Serializes value to JSON string"));
            return items;
        }

        if (prefix.size() >= 3 && prefix.substr(prefix.size() - 3) == "fs.") {
            items.push_back(makeCompletionItem("read", 3, "fs.read(path) -> string", "Reads file contents as text"));
            items.push_back(makeCompletionItem("write", 3, "fs.write(path, text) -> null", "Writes text to file"));
            items.push_back(makeCompletionItem("exists", 3, "fs.exists(path) -> bool", "Checks if path exists"));
            items.push_back(makeCompletionItem("remove", 3, "fs.remove(path) -> null", "Removes file or directory"));
            items.push_back(makeCompletionItem("mkdir", 3, "fs.mkdir(path) -> null", "Creates directory"));
            items.push_back(makeCompletionItem("readdir", 3, "fs.readdir(path) -> array", "Lists files in directory"));
            return items;
        }

        if (prefix.size() >= 9 && prefix.substr(prefix.size() - 9) == "datetime.") {
            items.push_back(makeCompletionItem("now", 3, "datetime.now() -> object", "Returns current local date and time object"));
            items.push_back(makeCompletionItem("timestamp", 3, "datetime.timestamp() -> number", "Returns UNIX timestamp in seconds"));
            items.push_back(makeCompletionItem("format", 3, "datetime.format(ts, pattern) -> string", "Formats timestamp using pattern"));
            return items;
        }

        if (prefix.size() >= 5 && prefix.substr(prefix.size() - 5) == "path.") {
            items.push_back(makeCompletionItem("join", 3, "path.join(...parts) -> string", "Joins path segments with system separator"));
            items.push_back(makeCompletionItem("basename", 3, "path.basename(p) -> string", "Returns filename component"));
            items.push_back(makeCompletionItem("dirname", 3, "path.dirname(p) -> string", "Returns directory component"));
            items.push_back(makeCompletionItem("ext", 3, "path.ext(p) -> string", "Returns file extension including dot"));
            items.push_back(makeCompletionItem("stem", 3, "path.stem(p) -> string", "Returns filename without extension"));
            items.push_back(makeCompletionItem("exists", 3, "path.exists(p) -> bool", "Checks if path exists"));
            items.push_back(makeCompletionItem("abs", 3, "path.abs(p) -> string", "Resolves absolute path"));
            return items;
        }

        if (prefix.size() >= 4 && prefix.substr(prefix.size() - 4) == "env.") {
            items.push_back(makeCompletionItem("get", 3, "env.get(name) -> string", "Gets environment variable value"));
            items.push_back(makeCompletionItem("set", 3, "env.set(name, val) -> null", "Sets environment variable"));
            items.push_back(makeCompletionItem("has", 3, "env.has(name) -> bool", "Checks if environment variable is set"));
            items.push_back(makeCompletionItem("all", 3, "env.all() -> object", "Returns all environment variables as key-value map"));
            return items;
        }

        if (prefix.size() >= 8 && prefix.substr(prefix.size() - 8) == "process.") {
            items.push_back(makeCompletionItem("args", 3, "process.args() -> array", "Returns CLI arguments array"));
            items.push_back(makeCompletionItem("cwd", 3, "process.cwd() -> string", "Returns current working directory"));
            items.push_back(makeCompletionItem("pid", 3, "process.pid() -> number", "Returns process ID"));
            items.push_back(makeCompletionItem("exit", 3, "process.exit(code) -> null", "Terminates current process"));
            return items;
        }

        if (prefix.size() >= 7 && prefix.substr(prefix.size() - 7) == "crypto.") {
            items.push_back(makeCompletionItem("sha256", 3, "crypto.sha256(str) -> string", "Computes SHA-256 hex digest"));
            items.push_back(makeCompletionItem("sha1", 3, "crypto.sha1(str) -> string", "Computes SHA-1 hex digest"));
            items.push_back(makeCompletionItem("md5", 3, "crypto.md5(str) -> string", "Computes MD5 hex digest"));
            items.push_back(makeCompletionItem("uuid", 3, "crypto.uuid() -> string", "Generates UUID v4"));
            items.push_back(makeCompletionItem("base64_encode", 3, "crypto.base64_encode(str) -> string", "Encodes string to Base64"));
            items.push_back(makeCompletionItem("base64_decode", 3, "crypto.base64_decode(b64) -> string", "Decodes Base64 to string"));
            return items;
        }

        if (prefix.size() >= 7 && prefix.substr(prefix.size() - 7) == "engine.") {
            items.push_back(makeCompletionItem("is_editor", 3, "engine.is_editor() -> bool", "Returns true if running in editor mode"));
            items.push_back(makeCompletionItem("is_playing", 3, "engine.is_playing() -> bool", "Returns true if game loop is active"));
            items.push_back(makeCompletionItem("play", 3, "engine.play() -> null", "Starts engine play mode"));
            items.push_back(makeCompletionItem("pause", 3, "engine.pause() -> null", "Pauses game execution"));
            items.push_back(makeCompletionItem("stop", 3, "engine.stop() -> null", "Stops play mode and restores scene"));
            items.push_back(makeCompletionItem("load_texture", 3, "engine.load_texture(path) -> texture", "Loads texture asset"));
            items.push_back(makeCompletionItem("draw_texture", 3, "engine.draw_texture(tex, x, y) -> null", "Draws texture at coordinates"));
            items.push_back(makeCompletionItem("load_sound", 3, "engine.load_sound(path) -> sound", "Loads audio sound file"));
            items.push_back(makeCompletionItem("play_sound", 3, "engine.play_sound(sound) -> null", "Plays sound effect"));
            return items;
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

        std::vector<std::string> namespaces = {
            "math", "json", "fs", "datetime", "path", "env", "process", "crypto", "engine"
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
                triggers.push_back("/");
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
