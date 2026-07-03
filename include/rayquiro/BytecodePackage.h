#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Bytecode.h"

class BytecodePackage {
public:
    static constexpr std::uint32_t kVersion = 1;

    static void writeToFile(const BytecodeProgram& program, const std::filesystem::path& path) {
        std::vector<std::uint8_t> payload = serializeProgram(program);
        const std::uint32_t checksum = fnv1a(payload);
        const std::uint32_t key = generateKey();
        obfuscate(payload, key);

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Cannot write bytecode package: " + path.string());
        }

        output.write("RQB1", 4);
        writeRaw(output, kVersion);
        writeRaw(output, static_cast<std::uint32_t>(1));
        writeRaw(output, key);
        writeRaw(output, static_cast<std::uint32_t>(payload.size()));
        writeRaw(output, checksum);
        if (!payload.empty()) {
            output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        }
    }

    static BytecodeProgram readFromFile(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot open bytecode package: " + path.string());
        }

        char magic[4] = {};
        input.read(magic, 4);
        if (input.gcount() != 4 || std::memcmp(magic, "RQB1", 4) != 0) {
            throw std::runtime_error("Invalid RayQuiro bytecode package.");
        }

        const std::uint32_t version = readRaw<std::uint32_t>(input);
        if (version != kVersion) {
            throw std::runtime_error("Unsupported bytecode package version: " + std::to_string(version));
        }

        const std::uint32_t flags = readRaw<std::uint32_t>(input);
        const std::uint32_t key = readRaw<std::uint32_t>(input);
        const std::uint32_t payloadSize = readRaw<std::uint32_t>(input);
        const std::uint32_t checksum = readRaw<std::uint32_t>(input);

        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize > 0) {
            input.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
            if (static_cast<std::size_t>(input.gcount()) != payload.size()) {
                throw std::runtime_error("Bytecode package payload is truncated.");
            }
        }

        if ((flags & 1u) != 0u) {
            obfuscate(payload, key);
        }

        if (fnv1a(payload) != checksum) {
            throw std::runtime_error("Bytecode package checksum mismatch.");
        }

        Reader reader(payload);
        return deserializeProgram(reader);
    }

private:
    class Writer {
    public:
        void writeBytes(const void* data, std::size_t size) {
            const auto* begin = reinterpret_cast<const std::uint8_t*>(data);
            bytes.insert(bytes.end(), begin, begin + size);
        }

        template <typename T>
        void writePod(const T& value) {
            writeBytes(&value, sizeof(T));
        }

        void writeString(const std::string& value) {
            writePod(static_cast<std::uint32_t>(value.size()));
            if (!value.empty()) {
                writeBytes(value.data(), value.size());
            }
        }

        std::vector<std::uint8_t> bytes;
    };

    class Reader {
    public:
        explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}

        template <typename T>
        T readPod() {
            require(sizeof(T));
            T value {};
            std::memcpy(&value, data_.data() + offset_, sizeof(T));
            offset_ += sizeof(T);
            return value;
        }

        std::string readString() {
            const std::uint32_t size = readPod<std::uint32_t>();
            require(size);
            std::string value;
            value.resize(size);
            if (size > 0) {
                std::memcpy(value.data(), data_.data() + offset_, size);
                offset_ += size;
            }
            return value;
        }

    private:
        void require(std::size_t size) {
            if (offset_ + size > data_.size()) {
                throw std::runtime_error("Corrupted bytecode package.");
            }
        }

        const std::vector<std::uint8_t>& data_;
        std::size_t offset_ = 0;
    };

    static std::uint32_t generateKey() {
        std::random_device device;
        const std::uint32_t randomSeed = device.entropy() > 0.0 ? device() : static_cast<std::uint32_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return randomSeed == 0 ? 0xA341316Cu : randomSeed;
    }

    template <typename T>
    static void writeRaw(std::ofstream& output, const T& value) {
        output.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template <typename T>
    static T readRaw(std::ifstream& input) {
        T value {};
        input.read(reinterpret_cast<char*>(&value), sizeof(T));
        if (input.gcount() != sizeof(T)) {
            throw std::runtime_error("Corrupted bytecode package header.");
        }
        return value;
    }

    static std::uint32_t fnv1a(const std::vector<std::uint8_t>& bytes) {
        std::uint32_t hash = 2166136261u;
        for (std::uint8_t byte : bytes) {
            hash ^= byte;
            hash *= 16777619u;
        }
        return hash;
    }

    static std::uint32_t nextMask(std::uint32_t& state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    static void obfuscate(std::vector<std::uint8_t>& bytes, std::uint32_t key) {
        std::uint32_t state = key == 0 ? 0x9E3779B9u : key;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if ((i & 3u) == 0u) {
                state = nextMask(state);
            }
            const std::uint8_t mask = static_cast<std::uint8_t>((state >> ((i & 3u) * 8u)) & 0xFFu);
            bytes[i] ^= mask;
        }
    }

    static std::vector<std::uint8_t> serializeProgram(const BytecodeProgram& program) {
        Writer writer;
        writeFunction(writer, program.entry);

        std::vector<std::pair<std::string, BytecodeFunction>> orderedFunctions;
        orderedFunctions.reserve(program.functions.size());
        for (const auto& [name, function] : program.functions) {
            orderedFunctions.push_back({name, function});
        }
        std::sort(orderedFunctions.begin(), orderedFunctions.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });

        writer.writePod(static_cast<std::uint32_t>(orderedFunctions.size()));
        for (const auto& [_, function] : orderedFunctions) {
            writeFunction(writer, function);
        }

        return std::move(writer.bytes);
    }

    static BytecodeProgram deserializeProgram(Reader& reader) {
        BytecodeProgram program;
        program.entry = readFunction(reader);

        const std::uint32_t functionCount = reader.readPod<std::uint32_t>();
        for (std::uint32_t i = 0; i < functionCount; ++i) {
            BytecodeFunction function = readFunction(reader);
            program.functions[function.name] = std::move(function);
        }

        return program;
    }

    static void writeFunction(Writer& writer, const BytecodeFunction& function) {
        writer.writeString(function.name);
        writer.writePod(static_cast<std::uint32_t>(function.params.size()));
        for (const auto& param : function.params) {
            writer.writeString(param);
        }

        writer.writePod(static_cast<std::uint32_t>(function.code.size()));
        for (const auto& instruction : function.code) {
            writer.writePod(static_cast<std::uint8_t>(instruction.op));
            writer.writePod(instruction.a);
            writer.writePod(instruction.b);
        }

        writer.writePod(static_cast<std::uint32_t>(function.constants.size()));
        for (const auto& constant : function.constants) {
            writeValue(writer, constant);
        }
    }

    static BytecodeFunction readFunction(Reader& reader) {
        BytecodeFunction function;
        function.name = reader.readString();

        const std::uint32_t paramCount = reader.readPod<std::uint32_t>();
        function.params.reserve(paramCount);
        for (std::uint32_t i = 0; i < paramCount; ++i) {
            function.params.push_back(reader.readString());
        }

        const std::uint32_t codeCount = reader.readPod<std::uint32_t>();
        function.code.reserve(codeCount);
        for (std::uint32_t i = 0; i < codeCount; ++i) {
            Instruction instruction;
            instruction.op = static_cast<OpCode>(reader.readPod<std::uint8_t>());
            instruction.a = reader.readPod<std::int32_t>();
            instruction.b = reader.readPod<std::int32_t>();
            function.code.push_back(instruction);
        }

        const std::uint32_t constantCount = reader.readPod<std::uint32_t>();
        function.constants.reserve(constantCount);
        for (std::uint32_t i = 0; i < constantCount; ++i) {
            function.constants.push_back(readValue(reader));
        }

        return function;
    }

    static void writeValue(Writer& writer, const VMValue& value) {
        if (std::holds_alternative<std::monostate>(value.data)) {
            writer.writePod(static_cast<std::uint8_t>(0));
            return;
        }
        if (std::holds_alternative<double>(value.data)) {
            writer.writePod(static_cast<std::uint8_t>(1));
            writer.writePod(std::get<double>(value.data));
            return;
        }
        if (std::holds_alternative<std::string>(value.data)) {
            writer.writePod(static_cast<std::uint8_t>(2));
            writer.writeString(std::get<std::string>(value.data));
            return;
        }
        if (std::holds_alternative<bool>(value.data)) {
            writer.writePod(static_cast<std::uint8_t>(3));
            writer.writePod(static_cast<std::uint8_t>(std::get<bool>(value.data) ? 1 : 0));
            return;
        }
        if (std::holds_alternative<VMValue::Array>(value.data)) {
            writer.writePod(static_cast<std::uint8_t>(4));
            const auto& items = std::get<VMValue::Array>(value.data);
            writer.writePod(static_cast<std::uint32_t>(items.size()));
            for (const auto& item : items) {
                writeValue(writer, item);
            }
            return;
        }
        if (std::holds_alternative<VMValue::Object>(value.data)) {
            writer.writePod(static_cast<std::uint8_t>(5));
            const auto& object = std::get<VMValue::Object>(value.data);
            writer.writePod(static_cast<std::uint32_t>(object.size()));
            std::vector<std::pair<std::string, VMValue>> entries;
            entries.reserve(object.size());
            for (const auto& [key, nested] : object) {
                entries.push_back({key, nested});
            }
            std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
            for (const auto& [key, nested] : entries) {
                writer.writeString(key);
                writeValue(writer, nested);
            }
            return;
        }

        throw std::runtime_error("Unsupported constant value in bytecode package.");
    }

    static VMValue readValue(Reader& reader) {
        switch (reader.readPod<std::uint8_t>()) {
        case 0:
            return VMValue();
        case 1:
            return VMValue(reader.readPod<double>());
        case 2:
            return VMValue(reader.readString());
        case 3:
            return VMValue(reader.readPod<std::uint8_t>() != 0);
        case 4: {
            const std::uint32_t size = reader.readPod<std::uint32_t>();
            VMValue::Array items;
            items.reserve(size);
            for (std::uint32_t i = 0; i < size; ++i) {
                items.push_back(readValue(reader));
            }
            return VMValue(items);
        }
        case 5: {
            const std::uint32_t size = reader.readPod<std::uint32_t>();
            VMValue::Object object;
            for (std::uint32_t i = 0; i < size; ++i) {
                const std::string key = reader.readString();
                object[key] = readValue(reader);
            }
            return VMValue(object);
        }
        default:
            throw std::runtime_error("Unsupported value tag in bytecode package.");
        }
    }
};
