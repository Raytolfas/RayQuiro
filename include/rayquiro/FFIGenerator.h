#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cctype>
#include <algorithm>

struct CFnSignature {
    std::string retType;
    std::string name;
    std::vector<std::pair<std::string, std::string>> params;
};

class FFIGenerator {
public:
    static std::string generate(const std::string& headerContent, const std::string& libName = "native_lib") {
        FFIGenerator gen;
        gen.libName_ = libName;
        gen.parseHeader(headerContent);
        return gen.emitRQ();
    }

private:
    std::string libName_;
    std::vector<CFnSignature> signatures_;

    void parseHeader(const std::string& content) {
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line.rfind("extern \"C\"", 0) == 0) continue;
            size_t openParen = line.find('(');
            size_t closeParen = line.find(')');
            if (openParen == std::string::npos || closeParen == std::string::npos || closeParen < openParen) continue;
            size_t semi = line.find(';', closeParen);
            if (semi == std::string::npos) continue;

            std::string beforeParen = trim(line.substr(0, openParen));
            size_t lastSpace = beforeParen.rfind(' ');
            if (lastSpace == std::string::npos) lastSpace = beforeParen.rfind('*');
            if (lastSpace == std::string::npos) continue;

            std::string retType = trim(beforeParen.substr(0, lastSpace));
            std::string fnName = trim(beforeParen.substr(lastSpace + 1));
            while (!fnName.empty() && fnName[0] == '*') fnName = fnName.substr(1);
            if (fnName.empty()) continue;

            std::string insideParens = line.substr(openParen + 1, closeParen - openParen - 1);
            std::vector<std::pair<std::string, std::string>> params;
            if (trim(insideParens) != "void" && !trim(insideParens).empty()) {
                std::istringstream paramStream(insideParens);
                std::string param;
                int paramIdx = 0;
                while (std::getline(paramStream, param, ',')) {
                    param = trim(param);
                    if (param.empty()) continue;
                    size_t pSpace = param.rfind(' ');
                    if (pSpace == std::string::npos) pSpace = param.rfind('*');
                    std::string pType, pName;
                    if (pSpace != std::string::npos) {
                        pType = trim(param.substr(0, pSpace));
                        pName = trim(param.substr(pSpace + 1));
                        while (!pName.empty() && pName[0] == '*') pName = pName.substr(1);
                    } else {
                        pType = param;
                        pName = "arg" + std::to_string(++paramIdx);
                    }
                    if (pName.empty()) pName = "arg" + std::to_string(++paramIdx);
                    params.push_back({pType, pName});
                }
            }
            signatures_.push_back({retType, fnName, params});
        }
    }

    std::string mapCTypeToRQ(const std::string& cType) {
        if (cType == "void") return "void";
        if (cType == "int" || cType == "short" || cType == "long" || cType == "int32_t" || cType == "int64_t" || cType == "size_t") return "int";
        if (cType == "float" || cType == "double") return "float";
        if (cType == "bool" || cType == "_Bool") return "bool";
        if (cType == "char*" || cType == "const char*") return "string";
        return "any";
    }

    std::string emitRQ() {
        std::ostringstream out;
        out << "import ffi;\n\n";
        out << "const _lib = ffi.load(\"" << libName_ << "\");\n\n";
        for (const auto& sig : signatures_) {
            std::string rqRet = mapCTypeToRQ(sig.retType);
            out << "pub fn " << sig.name << "(";
            for (size_t i = 0; i < sig.params.size(); ++i) {
                if (i) out << ", ";
                out << sig.params[i].second << ": " << mapCTypeToRQ(sig.params[i].first);
            }
            out << ")";
            if (rqRet != "void") out << ": " << rqRet;
            out << " {\n";
            out << "    ";
            if (rqRet != "void") out << "return ";
            out << "_lib.call(\"" << sig.name << "\"";
            for (const auto& p : sig.params) {
                out << ", " << p.second;
            }
            out << ");\n";
            out << "}\n\n";
        }
        return out.str();
    }

    static std::string trim(const std::string& s) {
        auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
        auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
        return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
    }
};
