#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <regex>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class Log {
public:
    static void init() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (hErr != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hErr, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hErr, dwMode);
            }
        }
#endif
    }

    static void status(const std::string& verb, const std::string& message) {
        std::cout << "\033[1;32m" << padLeft(verb, 12) << "\033[0m " << message << "\n";
    }

    static void info(const std::string& message) {
        std::cout << "\033[1;36minfo:\033[0m " << message << "\n";
    }

    static void warn(const std::string& message) {
        std::cerr << "\033[1;33mwarning:\033[0m " << message << "\n";
    }

    static void error(const std::string& message) {
        std::cerr << "\033[1;31merror:\033[0m " << message << "\n";
    }

    static void diagnostic(
        const std::filesystem::path& file,
        int line,
        int col,
        const std::string& message,
        bool isWarning = false
    ) {
        std::ostream& out = std::cerr;
        std::string tag = isWarning ? "\033[1;33mwarning" : "\033[1;31merror";
        out << tag << ":\033[0m " << message << "\n";

        std::string filePath = file.string();
        if (!filePath.empty()) {
            out << "  \033[1;34m-->\033[0m " << filePath << ":" << line << ":" << col << "\n";
        }

        std::string sourceLine = getLineFromFile(file, line);
        if (!sourceLine.empty()) {
            std::string lineStr = std::to_string(line);
            std::string indent(lineStr.length() + 1, ' ');

            out << "  \033[1;34m" << indent << "|\033[0m\n";
            out << "  \033[1;34m" << lineStr << " |\033[0m " << sourceLine << "\n";

            int caretPos = (col > 0) ? (col - 1) : 0;
            std::string spaces = "";
            for (int i = 0; i < caretPos && i < (int)sourceLine.length(); ++i) {
                if (sourceLine[i] == '\t') spaces += "    ";
                else spaces += " ";
            }
            out << "  \033[1;34m" << indent << "|\033[0m " << spaces
                << (isWarning ? "\033[1;33m^\033[0m\n" : "\033[1;31m^\033[0m\n");
        }
    }

    static void reportException(const std::exception& e, const std::filesystem::path& fallbackFile = {}) {
        std::string raw = e.what();

        std::regex locRegex(R"((.*) at (?:([^:]+):)?(\d+):(\d+)\s*$)");
        std::smatch m;
        if (std::regex_search(raw, m, locRegex)) {
            std::string msg = m[1].str();
            std::string fileStr = m[2].matched ? m[2].str() : fallbackFile.string();
            int line = std::stoi(m[3].str());
            int col = std::stoi(m[4].str());
            diagnostic(fileStr, line, col, msg, false);
            return;
        }

        std::regex altRegex(R"((.*):(\d+):(\d+):\s*(.*))");
        if (std::regex_search(raw, m, altRegex)) {
            std::string fileStr = m[1].str();
            int line = std::stoi(m[2].str());
            int col = std::stoi(m[3].str());
            std::string msg = m[4].str();
            diagnostic(fileStr, line, col, msg, false);
            return;
        }

        error(raw);
    }

private:
    static std::string padLeft(const std::string& str, size_t totalWidth) {
        if (str.length() >= totalWidth) return str;
        return std::string(totalWidth - str.length(), ' ') + str;
    }

    static std::string getLineFromFile(const std::filesystem::path& file, int targetLine) {
        if (file.empty() || !std::filesystem::exists(file) || targetLine <= 0) {
            return "";
        }
        std::ifstream f(file);
        if (!f) return "";
        std::string line;
        int current = 1;
        while (std::getline(f, line)) {
            if (current == targetLine) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                    line.pop_back();
                }
                return line;
            }
            current++;
        }
        return "";
    }
};
