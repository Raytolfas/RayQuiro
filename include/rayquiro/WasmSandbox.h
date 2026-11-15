#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <cstring>

struct WasmExport {
    std::string name;
    uint32_t index;
    uint8_t kind;
};

class WasmSandboxInstance {
public:
    WasmSandboxInstance(std::vector<uint8_t> bytecode, size_t memoryLimitBytes, uint64_t instructionQuota)
        : rawBytes_(std::move(bytecode)), memoryLimit_(memoryLimitBytes), instructionQuota_(instructionQuota) {
        parseModule();
        memory_.resize(std::min(memoryLimit_, static_cast<size_t>(65536)));
    }

    void setMemoryLimit(size_t limit) {
        memoryLimit_ = limit;
        if (memory_.size() > memoryLimit_) {
            memory_.resize(memoryLimit_);
        }
    }

    void setInstructionQuota(uint64_t quota) {
        instructionQuota_ = quota;
    }

    size_t getMemorySize() const {
        return memory_.size();
    }

    bool hasExport(const std::string& name) const {
        return exports_.find(name) != exports_.end();
    }

    std::vector<std::string> getExports() const {
        std::vector<std::string> res;
        for (const auto& [name, exp] : exports_) {
            res.push_back(name);
        }
        return res;
    }

    double invoke(const std::string& name, const std::vector<double>& args) {
        auto it = exports_.find(name);
        if (it == exports_.end()) {
            throw std::runtime_error("WASM function not found: " + name);
        }
        instructionsExecuted_ = 0;
        return executeFunction(it->second.index, args);
    }

    std::vector<uint8_t>& getMemory() {
        return memory_;
    }

private:
    std::vector<uint8_t> rawBytes_;
    size_t memoryLimit_ = 16 * 1024 * 1024;
    uint64_t instructionQuota_ = 1000000;
    uint64_t instructionsExecuted_ = 0;
    std::vector<uint8_t> memory_;
    std::unordered_map<std::string, WasmExport> exports_;
    std::vector<std::vector<uint8_t>> functionBodies_;

    uint32_t readVarUint(size_t& offset) {
        uint32_t result = 0;
        uint32_t shift = 0;
        while (offset < rawBytes_.size()) {
            uint8_t byte = rawBytes_[offset++];
            result |= (byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }
        return result;
    }

    void parseModule() {
        if (rawBytes_.size() < 8) {
            throw std::runtime_error("Invalid WASM binary: size too small");
        }
        if (rawBytes_[0] != 0x00 || rawBytes_[1] != 0x61 || rawBytes_[2] != 0x73 || rawBytes_[3] != 0x6D) {
            throw std::runtime_error("Invalid WASM magic header");
        }
        size_t offset = 8;
        while (offset < rawBytes_.size()) {
            uint8_t sectionId = rawBytes_[offset++];
            uint32_t sectionLen = readVarUint(offset);
            size_t sectionEnd = offset + sectionLen;
            if (sectionEnd > rawBytes_.size()) break;

            if (sectionId == 7) {
                size_t expOffset = offset;
                uint32_t count = readVarUint(expOffset);
                for (uint32_t i = 0; i < count && expOffset < sectionEnd; ++i) {
                    uint32_t nameLen = readVarUint(expOffset);
                    if (expOffset + nameLen > sectionEnd) break;
                    std::string expName(reinterpret_cast<char*>(&rawBytes_[expOffset]), nameLen);
                    expOffset += nameLen;
                    uint8_t kind = rawBytes_[expOffset++];
                    uint32_t index = readVarUint(expOffset);
                    exports_[expName] = {expName, index, kind};
                }
            } else if (sectionId == 10) {
                size_t codeOffset = offset;
                uint32_t count = readVarUint(codeOffset);
                for (uint32_t i = 0; i < count && codeOffset < sectionEnd; ++i) {
                    uint32_t bodySize = readVarUint(codeOffset);
                    if (codeOffset + bodySize > sectionEnd) break;
                    functionBodies_.emplace_back(rawBytes_.begin() + codeOffset, rawBytes_.begin() + codeOffset + bodySize);
                    codeOffset += bodySize;
                }
            }
            offset = sectionEnd;
        }
    }

    double executeFunction(uint32_t fnIndex, const std::vector<double>& args) {
        if (fnIndex >= functionBodies_.size()) {
            double acc = 0.0;
            for (double a : args) acc += a;
            return acc;
        }
        const auto& body = functionBodies_[fnIndex];
        std::vector<double> stack;
        std::vector<double> locals = args;
        locals.resize(std::max(locals.size(), static_cast<size_t>(16)), 0.0);

        size_t ip = 0;
        uint32_t localDeclCount = 0;
        if (ip < body.size()) {
            localDeclCount = body[ip++];
            for (uint32_t i = 0; i < localDeclCount && ip < body.size(); ++i) {
                uint32_t count = body[ip++];
                uint8_t type = body[ip++];
                (void)type;
                for (uint32_t c = 0; c < count; ++c) locals.push_back(0.0);
            }
        }

        while (ip < body.size()) {
            if (++instructionsExecuted_ > instructionQuota_) {
                throw std::runtime_error("WASM sandbox execution quota exceeded");
            }
            uint8_t opcode = body[ip++];
            if (opcode == 0x0B) {
                break;
            } else if (opcode == 0x01) {
                continue;
            } else if (opcode == 0x20) {
                uint32_t idx = body[ip++];
                stack.push_back(idx < locals.size() ? locals[idx] : 0.0);
            } else if (opcode == 0x21) {
                uint32_t idx = body[ip++];
                if (!stack.empty()) {
                    double v = stack.back();
                    stack.pop_back();
                    if (idx < locals.size()) locals[idx] = v;
                }
            } else if (opcode == 0x22) {
                uint32_t idx = body[ip++];
                if (!stack.empty()) {
                    if (idx < locals.size()) locals[idx] = stack.back();
                }
            } else if (opcode == 0x41) {
                int32_t val = 0;
                uint32_t shift = 0;
                while (ip < body.size()) {
                    uint8_t b = body[ip++];
                    val |= (b & 0x7F) << shift;
                    if ((b & 0x80) == 0) break;
                    shift += 7;
                }
                stack.push_back(static_cast<double>(val));
            } else if (opcode == 0x6A) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(static_cast<double>(static_cast<int64_t>(a) + static_cast<int64_t>(b)));
                }
            } else if (opcode == 0x6B) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(static_cast<double>(static_cast<int64_t>(a) - static_cast<int64_t>(b)));
                }
            } else if (opcode == 0x6C) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(static_cast<double>(static_cast<int64_t>(a) * static_cast<int64_t>(b)));
                }
            } else if (opcode == 0x6D) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(b != 0.0 ? static_cast<double>(static_cast<int64_t>(a) / static_cast<int64_t>(b)) : 0.0);
                }
            } else if (opcode == 0xA0) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(a + b);
                }
            } else if (opcode == 0xA1) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(a - b);
                }
            } else if (opcode == 0xA2) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(a * b);
                }
            } else if (opcode == 0xA3) {
                if (stack.size() >= 2) {
                    double b = stack.back(); stack.pop_back();
                    double a = stack.back(); stack.pop_back();
                    stack.push_back(b != 0.0 ? a / b : 0.0);
                }
            } else if (opcode == 0x0F) {
                break;
            }
        }
        return stack.empty() ? 0.0 : stack.back();
    }
};

class WasmManager {
public:
    static WasmManager& instance() {
        static WasmManager mgr;
        return mgr;
    }

    int64_t create(std::vector<uint8_t> bytes, size_t memLimit = 16 * 1024 * 1024, uint64_t quota = 1000000) {
        int64_t id = ++counter_;
        sandboxes_[id] = std::make_shared<WasmSandboxInstance>(std::move(bytes), memLimit, quota);
        return id;
    }

    std::shared_ptr<WasmSandboxInstance> get(int64_t id) {
        auto it = sandboxes_.find(id);
        return it != sandboxes_.end() ? it->second : nullptr;
    }

    bool remove(int64_t id) {
        return sandboxes_.erase(id) > 0;
    }

private:
    int64_t counter_ = 0;
    std::unordered_map<int64_t, std::shared_ptr<WasmSandboxInstance>> sandboxes_;
};
