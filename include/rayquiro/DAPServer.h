#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32

using socket_t = SOCKET;
static const socket_t INVALID_SOCK = INVALID_SOCKET;
static inline int close_sock(socket_t s) { return closesocket(s); }
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using socket_t = int;
static constexpr socket_t INVALID_SOCK = -1;
static inline int close_sock(socket_t s) { return ::close(s); }
#endif

#include "nlohmann_json.h"

struct DAPBreakpoint {
    std::string path;
    int         line;
    bool        verified = false;
};

class DAPServer {
public:

    using StepHook = std::function<void(const std::string& filePath, int line)>;

    explicit DAPServer(int port = 4711) : port_(port) {}
    ~DAPServer() { stop(); }

    bool start();
    void stop();

    bool shouldPause(const std::string& filePath, int line);

    std::string waitForResume();

    void sendEvent(const std::string& event, const json& body = {});

    void sendOutput(const std::string& message, const std::string& category = "stdout");

    void setBreakpoints(const std::string& path, const std::vector<int>& lines);

    struct StackFrame {
        std::string name;
        std::string sourcePath;
        int         line = 0;
    };
    void setStackFrames(std::vector<StackFrame> frames);

    void setVariables(const std::map<std::string, std::string>& vars);

    bool isConnected() const { return connected_.load(); }

private:
    int    port_;
    socket_t serverSock_ = INVALID_SOCK;
    socket_t clientSock_ = INVALID_SOCK;

    std::atomic<bool> running_  {false};
    std::atomic<bool> connected_{false};

    std::mutex              pauseMutex_;
    std::condition_variable pauseCv_;
    bool                    paused_  = false;
    std::string             resumeCmd_;

    enum class StepMode { None, Continue, Next, StepIn, StepOut };
    std::atomic<StepMode> stepMode_{StepMode::Continue};
    std::string           lastPausedPath_;
    int                   lastPausedLine_ = 0;

    std::mutex                         bpMutex_;
    std::map<std::string, std::set<int>> breakpoints_;

    std::mutex               frameMutex_;
    std::vector<StackFrame>  stackFrames_;
    std::map<std::string, std::string> variables_;

    int seq_ = 1;

    bool sendRaw(const std::string& data);
    std::optional<json> recvMessage();
    void sendResponse(const json& request, bool success, const json& body = {});

    void handleMessage(const json& msg);
    void handleInitialize(const json& req);
    void handleLaunch(const json& req);
    void handleAttach(const json& req);
    void handleSetBreakpoints(const json& req);
    void handleSetExceptionBreakpoints(const json& req);
    void handleThreads(const json& req);
    void handleStackTrace(const json& req);
    void handleScopes(const json& req);
    void handleVariables(const json& req);
    void handleContinue(const json& req);
    void handleNext(const json& req);
    void handleStepIn(const json& req);
    void handleStepOut(const json& req);
    void handleDisconnect(const json& req);
    void handleConfigurationDone(const json& req);

    void readerLoop();
};

inline bool DAPServer::start() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(0x0202 , &wsaData);
#endif

    serverSock_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock_ == INVALID_SOCK) return false;

    int opt = 1;
    setsockopt(serverSock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (::bind(serverSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_sock(serverSock_);
        serverSock_ = INVALID_SOCK;
        return false;
    }
    if (::listen(serverSock_, 1) < 0) {
        close_sock(serverSock_);
        serverSock_ = INVALID_SOCK;
        return false;
    }

    running_ = true;
    std::cerr << "[DAP] Waiting for debugger on port " << port_ << " ...\n";

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    clientSock_ = ::accept(serverSock_,
                            reinterpret_cast<sockaddr*>(&clientAddr),
                            &clientLen);
    if (clientSock_ == INVALID_SOCK) {
        running_ = false;
        return false;
    }

    connected_ = true;
    std::cerr << "[DAP] Debugger connected.\n";

    std::thread([this]() { readerLoop(); }).detach();

    return true;
}

inline void DAPServer::stop() {
    running_   = false;
    connected_ = false;
    if (clientSock_ != INVALID_SOCK) { close_sock(clientSock_); clientSock_ = INVALID_SOCK; }
    if (serverSock_ != INVALID_SOCK) { close_sock(serverSock_); serverSock_ = INVALID_SOCK; }
    pauseCv_.notify_all();
#ifdef _WIN32
    WSACleanup();
#endif
}

inline bool DAPServer::shouldPause(const std::string& filePath, int line) {
    if (!connected_) return false;

    StepMode mode = stepMode_.load();

    {
        std::lock_guard<std::mutex> lock(bpMutex_);
        auto it = breakpoints_.find(filePath);
        if (it != breakpoints_.end() && it->second.count(line)) {
            stepMode_ = StepMode::None;
            return true;
        }
    }

    if (mode == StepMode::Next || mode == StepMode::StepIn) {
        if (filePath != lastPausedPath_ || line != lastPausedLine_) {
            stepMode_ = StepMode::None;
            return true;
        }
    }

    return false;
}

inline std::string DAPServer::waitForResume() {

    {
        std::lock_guard<std::mutex> flock(frameMutex_);
        std::string path  = stackFrames_.empty() ? "" : stackFrames_.front().sourcePath;
        int         ln    = stackFrames_.empty() ? 0  : stackFrames_.front().line;
        lastPausedPath_ = path;
        lastPausedLine_ = ln;
    }
    sendEvent("stopped", {
        {"reason",    "breakpoint"},
        {"threadId",  1},
        {"allThreadsStopped", true}
    });

    {
        std::unique_lock<std::mutex> lock(pauseMutex_);
        paused_ = true;
        pauseCv_.wait(lock, [this] { return !paused_ || !running_; });
    }

    return resumeCmd_;
}

inline void DAPServer::setBreakpoints(const std::string& path, const std::vector<int>& lines) {
    std::lock_guard<std::mutex> lock(bpMutex_);
    breakpoints_[path].clear();
    for (int l : lines) breakpoints_[path].insert(l);
}

inline void DAPServer::setStackFrames(std::vector<StackFrame> frames) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    stackFrames_ = std::move(frames);
}

inline void DAPServer::setVariables(const std::map<std::string, std::string>& vars) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    variables_ = vars;
}

inline bool DAPServer::sendRaw(const std::string& data) {
    if (clientSock_ == INVALID_SOCK) return false;
    const std::string header = "Content-Length: " + std::to_string(data.size()) + "\r\n\r\n";
    const std::string msg    = header + data;
    int sent = ::send(clientSock_, msg.c_str(), static_cast<int>(msg.size()), 0);
    return sent == static_cast<int>(msg.size());
}

inline void DAPServer::sendEvent(const std::string& event, const json& body) {
    json msg = {
        {"seq",   seq_++},
        {"type",  "event"},
        {"event", event},
        {"body",  body}
    };
    sendRaw(msg.dump());
}

inline void DAPServer::sendOutput(const std::string& message, const std::string& category) {
    sendEvent("output", {{"category", category}, {"output", message + "\n"}});
}

inline void DAPServer::sendResponse(const json& req, bool success, const json& body) {
    json resp = {
        {"seq",         seq_++},
        {"type",        "response"},
        {"request_seq", req.value("seq", 0)},
        {"success",     success},
        {"command",     req.value("command", "")},
        {"body",        body}
    };
    sendRaw(resp.dump());
}

inline std::optional<json> DAPServer::recvMessage() {

    std::string header;
    char c;
    while (running_) {
        int n = ::recv(clientSock_, &c, 1, 0);
        if (n <= 0) return std::nullopt;
        header += c;
        if (header.size() >= 4 &&
            header.substr(header.size() - 4) == "\r\n\r\n") break;
    }

    int contentLength = 0;
    const std::string prefix = "Content-Length: ";
    auto pos = header.find(prefix);
    if (pos != std::string::npos) {
        contentLength = std::stoi(header.substr(pos + prefix.size()));
    }
    if (contentLength <= 0) return std::nullopt;

    std::string body(static_cast<std::size_t>(contentLength), '\0');
    int received = 0;
    while (received < contentLength && running_) {
        int n = ::recv(clientSock_, &body[static_cast<std::size_t>(received)],
                       contentLength - received, 0);
        if (n <= 0) return std::nullopt;
        received += n;
    }

    try {
        return json::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

inline void DAPServer::readerLoop() {
    while (running_ && connected_) {
        auto msg = recvMessage();
        if (!msg) break;
        handleMessage(*msg);
    }
    connected_ = false;
    pauseCv_.notify_all();
}

inline void DAPServer::handleMessage(const json& msg) {
    const std::string type    = msg.value("type", "");
    const std::string command = msg.value("command", "");

    if (type != "request") return;

    if      (command == "initialize")             handleInitialize(msg);
    else if (command == "launch")                 handleLaunch(msg);
    else if (command == "attach")                 handleAttach(msg);
    else if (command == "setBreakpoints")         handleSetBreakpoints(msg);
    else if (command == "setExceptionBreakpoints")handleSetExceptionBreakpoints(msg);
    else if (command == "configurationDone")      handleConfigurationDone(msg);
    else if (command == "threads")                handleThreads(msg);
    else if (command == "stackTrace")             handleStackTrace(msg);
    else if (command == "scopes")                 handleScopes(msg);
    else if (command == "variables")              handleVariables(msg);
    else if (command == "continue")               handleContinue(msg);
    else if (command == "next")                   handleNext(msg);
    else if (command == "stepIn")                 handleStepIn(msg);
    else if (command == "stepOut")                handleStepOut(msg);
    else if (command == "disconnect")             handleDisconnect(msg);
    else {

        sendResponse(msg, true);
    }
}

inline void DAPServer::handleInitialize(const json& req) {
    sendResponse(req, true, {
        {"supportsConfigurationDoneRequest",    true},
        {"supportsSetBreakpointsRequest",       true},
        {"supportsStepBack",                    false},
        {"supportsTerminateRequest",            true},
        {"supportsFunctionBreakpoints",         false},
        {"supportsExceptionInfoRequest",        false},
        {"supportsDelayedStackTraceLoading",    false}
    });
    sendEvent("initialized");
}

inline void DAPServer::handleLaunch(const json& req) {
    sendResponse(req, true);
}

inline void DAPServer::handleAttach(const json& req) {
    sendResponse(req, true);
}

inline void DAPServer::handleConfigurationDone(const json& req) {
    sendResponse(req, true);

    pauseCv_.notify_all();
}

inline void DAPServer::handleSetBreakpoints(const json& req) {
    const json& args = req.value("arguments", json::object());
    const json& source = args.value("source", json::object());
    const std::string path = source.value("path", "");
    std::vector<int> lines;
    json bps = json::array();
    if (args.contains("breakpoints") && args["breakpoints"].is_array()) {
        for (const auto& bp : args["breakpoints"]) {
            int ln = bp.value("line", 0);
            lines.push_back(ln);
            bps.push_back({{"id", ln}, {"verified", true}, {"line", ln}});
        }
    }
    setBreakpoints(path, lines);
    sendResponse(req, true, {{"breakpoints", bps}});
}

inline void DAPServer::handleSetExceptionBreakpoints(const json& req) {
    sendResponse(req, true, {{"breakpoints", json::array()}});
}

inline void DAPServer::handleThreads(const json& req) {
    sendResponse(req, true, {
        {"threads", json::array({{{"id", 1}, {"name", "main"}}})}
    });
}

inline void DAPServer::handleStackTrace(const json& req) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    json frames = json::array();
    int id = 0;
    for (const auto& f : stackFrames_) {
        frames.push_back({
            {"id",     id++},
            {"name",   f.name},
            {"line",   f.line},
            {"column", 1},
            {"source", {{"path", f.sourcePath}, {"name", f.sourcePath}}}
        });
    }
    sendResponse(req, true, {
        {"stackFrames", frames},
        {"totalFrames", static_cast<int>(stackFrames_.size())}
    });
}

inline void DAPServer::handleScopes(const json& req) {
    sendResponse(req, true, {
        {"scopes", json::array({{
            {"name",               "Locals"},
            {"variablesReference", 1},
            {"expensive",          false}
        }})}
    });
}

inline void DAPServer::handleVariables(const json& req) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    json vars = json::array();
    for (const auto& [name, val] : variables_) {
        vars.push_back({
            {"name",               name},
            {"value",              val},
            {"type",               ""},
            {"variablesReference", 0}
        });
    }
    sendResponse(req, true, {{"variables", vars}});
}

inline void DAPServer::handleContinue(const json& req) {
    sendResponse(req, true, {{"allThreadsContinued", true}});
    stepMode_ = StepMode::Continue;
    {
        std::lock_guard<std::mutex> lock(pauseMutex_);
        paused_    = false;
        resumeCmd_ = "continue";
    }
    pauseCv_.notify_all();
}

inline void DAPServer::handleNext(const json& req) {
    sendResponse(req, true);
    stepMode_ = StepMode::Next;
    {
        std::lock_guard<std::mutex> lock(pauseMutex_);
        paused_    = false;
        resumeCmd_ = "next";
    }
    pauseCv_.notify_all();
}

inline void DAPServer::handleStepIn(const json& req) {
    sendResponse(req, true);
    stepMode_ = StepMode::StepIn;
    {
        std::lock_guard<std::mutex> lock(pauseMutex_);
        paused_    = false;
        resumeCmd_ = "stepIn";
    }
    pauseCv_.notify_all();
}

inline void DAPServer::handleStepOut(const json& req) {
    sendResponse(req, true);
    stepMode_ = StepMode::Continue;
    {
        std::lock_guard<std::mutex> lock(pauseMutex_);
        paused_    = false;
        resumeCmd_ = "stepOut";
    }
    pauseCv_.notify_all();
}

inline void DAPServer::handleDisconnect(const json& req) {
    sendResponse(req, true);
    stop();
}

