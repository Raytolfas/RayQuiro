#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <variant>
#include <vector>

struct VMValue {
    using Array = std::vector<VMValue>;
    using Object = std::map<std::string, VMValue>;


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
    BuildObject,   // pop 2*n (key, val pairs) from stack → push object
    GetIndex,      // stack: [obj, key] → push obj[key]
    SetIndex,      // stack: [obj, key, val] → obj[key]=val, push val
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
    Return,
    Throw,         // stack: [msg] → throw runtime_error(to_string(msg))
    Concat,        // instruction.a = n values → pop n, concat to string, push
    TryBegin,      // instruction.a = catch_addr; instruction.b = error_name_const_idx
    TryEnd,        // instruction.a = after_catch_addr (jump over catch body)
    Dup,           // duplicate top of stack
    JumpIfNotNull, // instruction.b = target; jump if top-of-stack is NOT null (leaves value)
    SetGlobalIndex, // instruction.a = const-idx of name; stack: [key, val] → globals[name][key]=val; push val
    SetLocalIndex,  // instruction.a = local slot;         stack: [key, val] → locals[slot][key]=val; push val
    AppendGlobal,   // instruction.a = const-idx of name;  stack: [val] → globals[name].push_back(val); push val
    AppendLocal,    // instruction.a = local slot;          stack: [val] → locals[slot].push_back(val); push val
    AppendObjGlobal,// instruction.a = const-idx of name;  stack: [key, val] → globals[name][key].push_back(val); push val
    AppendObjLocal, // instruction.a = local slot;          stack: [key, val] → locals[slot][key].push_back(val); push val
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
    int localCount = 0;  // total local slots needed (set by compiler)
};

struct BytecodeProgram {
    BytecodeFunction entry;
    std::unordered_map<std::string, BytecodeFunction> functions;
};
