#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct VMValue {
    using Array = std::vector<VMValue>;
    using Object = std::unordered_map<std::string, VMValue>;

    std::variant<std::monostate, double, std::string, bool, Array, Object> data;

    VMValue() : data(std::monostate{}) {}
    VMValue(double value) : data(value) {}
    VMValue(const std::string& value) : data(value) {}
    VMValue(const char* value) : data(std::string(value)) {}
    VMValue(bool value) : data(value) {}
    VMValue(const Array& value) : data(value) {}
    VMValue(const Object& value) : data(value) {}
};

enum class OpCode : std::uint8_t {
    Constant,
    Null,
    True,
    False,
    BuildArray,
    GetIndex,
    Pop,
    DefineGlobal,
    GetGlobal,
    SetGlobal,
    GetLocal,
    SetLocal,
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Negate,
    Not,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Jump,
    JumpIfFalse,
    Loop,
    Call,
    Return
};

struct Instruction {
    OpCode op = OpCode::Return;
    std::int32_t a = 0;
    std::int32_t b = 0;
};

struct BytecodeFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instruction> code;
    std::vector<VMValue> constants;
};

struct BytecodeProgram {
    BytecodeFunction entry;
    std::unordered_map<std::string, BytecodeFunction> functions;
};
