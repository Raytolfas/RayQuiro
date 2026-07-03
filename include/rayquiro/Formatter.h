#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Formatter {
public:
    static std::string formatSource(const std::string& source) {
        std::vector<std::string> lines = splitLines(source);
        std::ostringstream output;
        int indent = 0;

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = trim(lines[i]);
            if (line.empty()) {
                output << '\n';
                continue;
            }

            const int leadingClosers = countLeadingClosers(line);
            const int safeIndent = std::max(0, indent - leadingClosers);

            output << std::string(static_cast<size_t>(safeIndent) * 4, ' ') << line;
            if (i + 1 < lines.size()) {
                output << '\n';
            }

            const int totalOpens = static_cast<int>(std::count(line.begin(), line.end(), '{'));
            const int totalCloses = static_cast<int>(std::count(line.begin(), line.end(), '}'));
            indent = std::max(0, safeIndent + totalOpens - std::max(0, totalCloses - leadingClosers));
        }

        return output.str();
    }

    static void formatFile(const std::filesystem::path& filePath) {
        std::ifstream input(filePath, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot open source file for formatting: " + filePath.string());
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string formatted = formatSource(buffer.str());

        std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
        output << formatted;
    }

private:
    static std::vector<std::string> splitLines(const std::string& source) {
        std::vector<std::string> lines;
        std::stringstream stream(source);
        std::string line;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }

        if (!source.empty() && (source.back() == '\n' || source.back() == '\r')) {
            lines.push_back("");
        }

        return lines;
    }

    static std::string trim(const std::string& value) {
        return rtrim(ltrim(value));
    }

    static std::string ltrim(const std::string& value) {
        size_t index = 0;
        while (index < value.size() && std::isspace(static_cast<unsigned char>(value[index])) != 0) {
            ++index;
        }
        return value.substr(index);
    }

    static std::string rtrim(const std::string& value) {
        if (value.empty()) {
            return value;
        }

        size_t index = value.size();
        while (index > 0 && std::isspace(static_cast<unsigned char>(value[index - 1])) != 0) {
            --index;
        }
        return value.substr(0, index);
    }

    static int countLeadingClosers(const std::string& line) {
        int count = 0;
        for (char c : line) {
            if (c == '}') {
                ++count;
                continue;
            }
            break;
        }
        return count;
    }
};
