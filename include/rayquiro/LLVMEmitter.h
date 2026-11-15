#pragma once

#include "Bytecode.h"
#include "LLVMToolchain.h"
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <algorithm>
#include <cmath>
#include <cctype>

class LLVMEmitter {
public:
    static std::string emit(const BytecodeProgram& program, const std::string& targetTriple = "") {
        LLVMEmitter emitter;
        emitter.program_ = &program;
        emitter.targetTriple_ = targetTriple.empty() ? LLVMToolchain::targetTriple() : targetTriple;
        emitter.emitProgram(program);
        return emitter.out_.str();
    }

private:
    std::ostringstream out_;
    const BytecodeProgram* program_ = nullptr;
    std::string targetTriple_;
    std::unordered_set<std::string> globalsDeclared_;
    int strConstCounter_ = 0;
    std::unordered_map<std::string, std::pair<std::string, size_t>> stringConstants_;

    static std::string safeName(const std::string& n) {
        std::string r;
        for (char c : n) {
            r += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
        }
        if (!r.empty() && std::isdigit((unsigned char)r[0])) r = "_" + r;
        return r;
    }

    std::string getStringConstant(const std::string& s) {
        auto it = stringConstants_.find(s);
        if (it != stringConstants_.end()) return it->second.first;
        std::string id = ".str." + std::to_string(++strConstCounter_);
        stringConstants_[s] = {id, s.size()};
        return id;
    }

    void emitHeader() {
        out_ << "; ModuleID = 'rayquiro_module'\n";
        out_ << "source_filename = \"rayquiro\"\n";
        if (targetTriple_.find("windows") != std::string::npos) {
            out_ << "target datalayout = \"e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"\n";
        } else if (targetTriple_.find("darwin") != std::string::npos || targetTriple_.find("apple") != std::string::npos) {
            out_ << "target datalayout = \"e-m:o-i64:64-i128:128-n32:64-S128\"\n";
        } else {
            out_ << "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"\n";
        }
        out_ << "target triple = \"" << targetTriple_ << "\"\n\n";

        out_ << "%struct.RqArray = type { ptr, i32, i32 }\n";
        out_ << "%struct.RqEntry = type { ptr, %struct.RqValue }\n";
        out_ << "%struct.RqObject = type { ptr, i32, i32 }\n";
        out_ << "%struct.RqBuffer = type { ptr, i32 }\n";
        out_ << "%union.RqData = type { double }\n";
        out_ << "%struct.RqValue = type { i32, %union.RqData }\n";
        out_ << "%struct.RqCatchFrame = type { ptr, i32, ptr }\n\n";

        out_ << "@_rq_catch_top = external global ptr, align 8\n";
        out_ << "@_g_pi = internal global %struct.RqValue zeroinitializer, align 8\n\n";

        out_ << "declare void @rq_runtime_init()\n";
        out_ << "declare void @rq_num(ptr sret(%struct.RqValue) align 8, double)\n";
        out_ << "declare void @rq_str(ptr sret(%struct.RqValue) align 8, ptr)\n";
        out_ << "declare void @rq_str_lit(ptr sret(%struct.RqValue) align 8, ptr)\n";
        out_ << "declare void @rq_bool(ptr sret(%struct.RqValue) align 8, i32)\n";
        out_ << "declare void @rq_null(ptr sret(%struct.RqValue) align 8)\n";
        out_ << "declare void @rq_arr_new(ptr sret(%struct.RqValue) align 8, i32)\n";
        out_ << "declare void @rq_obj_new(ptr sret(%struct.RqValue) align 8, i32)\n";
        out_ << "declare void @rq_push(ptr, ptr)\n";
        out_ << "declare void @rq_set_prop(ptr, ptr, ptr)\n";
        out_ << "declare void @rq_get_index(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_set_index(ptr, ptr, ptr)\n";
        out_ << "declare void @rq_add(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_sub(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_mul(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_div(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_mod(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_neg(ptr sret(%struct.RqValue) align 8, ptr)\n";
        out_ << "declare void @rq_not(ptr sret(%struct.RqValue) align 8, ptr)\n";
        out_ << "declare void @rq_eq(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_neq(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_lt(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_le(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_gt(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_ge(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_concat(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare void @rq_coalesce(ptr sret(%struct.RqValue) align 8, ptr, ptr)\n";
        out_ << "declare i32 @rq_truthy(ptr)\n";
        out_ << "declare void @rq_throw(ptr)\n";
        out_ << "declare void @rq_call_builtin(ptr sret(%struct.RqValue) align 8, ptr, i32, ptr)\n";
        out_ << "declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg)\n\n";
    }

    void collectGlobals(const BytecodeFunction& fn) {
        for (const auto& ins : fn.code) {
            auto op = ins.op;
            if (op == OpCode::TryBegin) {
                const auto& cval = fn.constants.at((size_t)ins.b);
                if (std::holds_alternative<std::string>(cval.data)) {
                    const std::string& name = std::get<std::string>(cval.data);
                    if (!name.empty() && globalsDeclared_.insert(name).second) {
                        out_ << "@_g_" << safeName(name) << " = internal global %struct.RqValue zeroinitializer, align 8\n";
                    }
                }
                continue;
            }
            bool isGlobal = (op == OpCode::DefineGlobal || op == OpCode::GetGlobal ||
                             op == OpCode::SetGlobal     || op == OpCode::SetGlobalIndex ||
                             op == OpCode::AppendGlobal  || op == OpCode::AppendObjGlobal);
            if (!isGlobal) continue;
            const auto& cval = fn.constants.at((size_t)ins.a);
            if (!std::holds_alternative<std::string>(cval.data)) continue;
            const std::string& name = std::get<std::string>(cval.data);
            if (globalsDeclared_.insert(name).second) {
                out_ << "@_g_" << safeName(name) << " = internal global %struct.RqValue zeroinitializer, align 8\n";
            }
        }
    }

    void emitStringConstants() {
        for (const auto& [str, entry] : stringConstants_) {
            const std::string& id = entry.first;
            size_t len = entry.second;
            out_ << "@" << id << " = private unnamed_addr constant [" << (len + 1) << " x i8] c\"";
            for (char c : str) {
                if (c == '\\') out_ << "\\5C";
                else if (c == '"') out_ << "\\22";
                else if (c == '\n') out_ << "\\0A";
                else if (c == '\r') out_ << "\\0D";
                else if (c == '\t') out_ << "\\09";
                else if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) >= 127) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\%02X", static_cast<unsigned char>(c));
                    out_ << buf;
                } else {
                    out_ << c;
                }
            }
            out_ << "\\00\", align 1\n";
        }
        if (!stringConstants_.empty()) out_ << "\n";
    }

    void emitFunctionSignature(const BytecodeFunction& fn, bool isDeclaration) {
        const std::string fname = fn.name.empty() ? "_rq_main" : "_rq_fn_" + safeName(fn.name);
        if (fn.name.empty()) {
            out_ << (isDeclaration ? "declare" : "define") << " void @" << fname << "()";
        } else {
            out_ << (isDeclaration ? "declare" : "define") << " void @" << fname << "(ptr dead_on_unwind noalias writable writeonly sret(%struct.RqValue) align 8 %_ret";
            for (size_t i = 0; i < fn.params.size(); ++i) {
                out_ << ", ptr readonly align 8 %_p" << i;
            }
            out_ << ")";
        }
    }

    void emitFunction(const BytecodeFunction& fn) {
        emitFunctionSignature(fn, false);
        out_ << " {\nentry:\n";

        int nlocals = std::max(fn.localCount, static_cast<int>(fn.params.size())) + 8;
        out_ << "  %_locals = alloca [" << nlocals << " x %struct.RqValue], align 8\n";
        out_ << "  %_stack = alloca [256 x %struct.RqValue], align 8\n";
        out_ << "  %_sp = alloca i32, align 4\n";
        out_ << "  store i32 0, ptr %_sp, align 4\n";
        out_ << "  %_tmp = alloca %struct.RqValue, align 8\n";

        for (int i = 0; i < nlocals; ++i) {
            out_ << "  %_loc_init_" << i << " = getelementptr inbounds [" << nlocals << " x %struct.RqValue], ptr %_locals, i64 0, i64 " << i << "\n";
            out_ << "  call void @rq_null(ptr sret(%struct.RqValue) align 8 %_loc_init_" << i << ")\n";
        }

        for (size_t i = 0; i < fn.params.size(); ++i) {
            out_ << "  %_param_dst_" << i << " = getelementptr inbounds [" << nlocals << " x %struct.RqValue], ptr %_locals, i64 0, i64 " << i << "\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %_param_dst_" << i << ", ptr align 8 %_p" << i << ", i64 16, i1 false)\n";
        }

        std::unordered_set<int> labels;
        for (const auto& ins : fn.code) {
            if (ins.op == OpCode::Jump || ins.op == OpCode::JumpIfFalse || ins.op == OpCode::Loop) labels.insert(ins.a);
            if (ins.op == OpCode::JumpIfNotNull) labels.insert(ins.b);
            if (ins.op == OpCode::TryBegin) labels.insert(ins.a);
            if (ins.op == OpCode::TryEnd) labels.insert(ins.a);
        }

        int regCounter = 0;
        out_ << "  br label %bb_0\n\n";

        for (size_t ip = 0; ip < fn.code.size(); ++ip) {
            out_ << "bb_" << ip << ":\n";
            emitInstruction(fn, ip, regCounter);
            bool isTerminator = false;
            auto op = fn.code[ip].op;
            if (op == OpCode::Jump || op == OpCode::Loop || op == OpCode::Return || op == OpCode::Throw || op == OpCode::JumpIfFalse || op == OpCode::JumpIfNotNull) {
                isTerminator = true;
            }
            if (!isTerminator) {
                out_ << "  br label %bb_" << (ip + 1) << "\n\n";
            } else {
                out_ << "\n";
            }
        }

        out_ << "bb_" << fn.code.size() << ":\n";
        if (fn.name.empty()) {
            out_ << "  ret void\n";
        } else {
            out_ << "  call void @rq_null(ptr sret(%struct.RqValue) align 8 %_ret)\n";
            out_ << "  ret void\n";
        }
        out_ << "}\n\n";
    }

    void emitInstruction(const BytecodeFunction& fn, size_t ip, int& reg) {
        const auto& ins = fn.code[ip];
        auto nextReg = [&]() { return "%r" + std::to_string(++reg); };

        auto pushStackFromPtr = [&](const std::string& srcPtr) {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string dst = nextReg();
            out_ << "  " << dst << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << sp << "\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << dst << ", ptr align 8 " << srcPtr << ", i64 16, i1 false)\n";
            std::string nextSp = nextReg();
            out_ << "  " << nextSp << " = add i32 " << sp << ", 1\n";
            out_ << "  store i32 " << nextSp << ", ptr %_sp, align 4\n";
        };

        auto popStackToPtr = [&](const std::string& dstPtr) {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string prevSp = nextReg();
            out_ << "  " << prevSp << " = sub i32 " << sp << ", 1\n";
            out_ << "  store i32 " << prevSp << ", ptr %_sp, align 4\n";
            std::string src = nextReg();
            out_ << "  " << src << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << prevSp << "\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << dstPtr << ", ptr align 8 " << src << ", i64 16, i1 false)\n";
        };

        switch (ins.op) {
        case OpCode::Constant: {
            const auto& val = fn.constants.at(static_cast<size_t>(ins.a));
            if (std::holds_alternative<std::monostate>(val.data)) {
                out_ << "  call void @rq_null(ptr sret(%struct.RqValue) align 8 %_tmp)\n";
            } else if (auto* d = std::get_if<double>(&val.data)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "0x%llx", *reinterpret_cast<const unsigned long long*>(d));
                out_ << "  call void @rq_num(ptr sret(%struct.RqValue) align 8 %_tmp, double " << buf << ")\n";
            } else if (auto* s = std::get_if<std::string>(&val.data)) {
                std::string strId = getStringConstant(*s);
                out_ << "  call void @rq_str_lit(ptr sret(%struct.RqValue) align 8 %_tmp, ptr @" << strId << ")\n";
            } else if (auto* b = std::get_if<bool>(&val.data)) {
                out_ << "  call void @rq_bool(ptr sret(%struct.RqValue) align 8 %_tmp, i32 " << (*b ? 1 : 0) << ")\n";
            }
            pushStackFromPtr("%_tmp");
            break;
        }
        case OpCode::Null: {
            out_ << "  call void @rq_null(ptr sret(%struct.RqValue) align 8 %_tmp)\n";
            pushStackFromPtr("%_tmp");
            break;
        }
        case OpCode::True: {
            out_ << "  call void @rq_bool(ptr sret(%struct.RqValue) align 8 %_tmp, i32 1)\n";
            pushStackFromPtr("%_tmp");
            break;
        }
        case OpCode::False: {
            out_ << "  call void @rq_bool(ptr sret(%struct.RqValue) align 8 %_tmp, i32 0)\n";
            pushStackFromPtr("%_tmp");
            break;
        }
        case OpCode::Pop: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string prevSp = nextReg();
            out_ << "  " << prevSp << " = sub i32 " << sp << ", 1\n";
            out_ << "  store i32 " << prevSp << ", ptr %_sp, align 4\n";
            break;
        }
        case OpCode::Dup: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string topIdx = nextReg();
            out_ << "  " << topIdx << " = sub i32 " << sp << ", 1\n";
            std::string topPtr = nextReg();
            out_ << "  " << topPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << topIdx << "\n";
            std::string dstPtr = nextReg();
            out_ << "  " << dstPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << sp << "\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << dstPtr << ", ptr align 8 " << topPtr << ", i64 16, i1 false)\n";
            std::string nextSp = nextReg();
            out_ << "  " << nextSp << " = add i32 " << sp << ", 1\n";
            out_ << "  store i32 " << nextSp << ", ptr %_sp, align 4\n";
            break;
        }
        case OpCode::GetLocal: {
            std::string locPtr = nextReg();
            out_ << "  " << locPtr << " = getelementptr inbounds [" << (std::max(fn.localCount, (int)fn.params.size()) + 8) << " x %struct.RqValue], ptr %_locals, i64 0, i32 " << ins.a << "\n";
            pushStackFromPtr(locPtr);
            break;
        }
        case OpCode::SetLocal: {
            std::string locPtr = nextReg();
            out_ << "  " << locPtr << " = getelementptr inbounds [" << (std::max(fn.localCount, (int)fn.params.size()) + 8) << " x %struct.RqValue], ptr %_locals, i64 0, i32 " << ins.a << "\n";
            popStackToPtr(locPtr);
            break;
        }
        case OpCode::DefineGlobal:
        case OpCode::SetGlobal: {
            const auto& cval = fn.constants.at(static_cast<size_t>(ins.a));
            const std::string& name = std::get<std::string>(cval.data);
            popStackToPtr("@_g_" + safeName(name));
            break;
        }
        case OpCode::GetGlobal: {
            const auto& cval = fn.constants.at(static_cast<size_t>(ins.a));
            const std::string& name = std::get<std::string>(cval.data);
            pushStackFromPtr("@_g_" + safeName(name));
            break;
        }
        case OpCode::Add:
        case OpCode::Subtract:
        case OpCode::Multiply:
        case OpCode::Divide:
        case OpCode::Modulo:
        case OpCode::Equal:
        case OpCode::NotEqual:
        case OpCode::Less:
        case OpCode::LessEqual:
        case OpCode::Greater:
        case OpCode::GreaterEqual:
        case OpCode::Concat: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string bIdx = nextReg();
            out_ << "  " << bIdx << " = sub i32 " << sp << ", 1\n";
            std::string aIdx = nextReg();
            out_ << "  " << aIdx << " = sub i32 " << sp << ", 2\n";
            std::string bPtr = nextReg();
            out_ << "  " << bPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << bIdx << "\n";
            std::string aPtr = nextReg();
            out_ << "  " << aPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << aIdx << "\n";

            std::string callee = "rq_add";
            if (ins.op == OpCode::Subtract) callee = "rq_sub";
            else if (ins.op == OpCode::Multiply) callee = "rq_mul";
            else if (ins.op == OpCode::Divide) callee = "rq_div";
            else if (ins.op == OpCode::Modulo) callee = "rq_mod";
            else if (ins.op == OpCode::Equal) callee = "rq_eq";
            else if (ins.op == OpCode::NotEqual) callee = "rq_neq";
            else if (ins.op == OpCode::Less) callee = "rq_lt";
            else if (ins.op == OpCode::LessEqual) callee = "rq_le";
            else if (ins.op == OpCode::Greater) callee = "rq_gt";
            else if (ins.op == OpCode::GreaterEqual) callee = "rq_ge";
            else if (ins.op == OpCode::Concat) callee = "rq_concat";

            out_ << "  call void @" << callee << "(ptr sret(%struct.RqValue) align 8 %_tmp, ptr " << aPtr << ", ptr " << bPtr << ")\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << aPtr << ", ptr align 8 %_tmp, i64 16, i1 false)\n";
            std::string nextSp = nextReg();
            out_ << "  " << nextSp << " = sub i32 " << sp << ", 1\n";
            out_ << "  store i32 " << nextSp << ", ptr %_sp, align 4\n";
            break;
        }
        case OpCode::Negate: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string topIdx = nextReg();
            out_ << "  " << topIdx << " = sub i32 " << sp << ", 1\n";
            std::string topPtr = nextReg();
            out_ << "  " << topPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << topIdx << "\n";
            out_ << "  call void @rq_neg(ptr sret(%struct.RqValue) align 8 %_tmp, ptr " << topPtr << ")\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << topPtr << ", ptr align 8 %_tmp, i64 16, i1 false)\n";
            break;
        }
        case OpCode::Not: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string topIdx = nextReg();
            out_ << "  " << topIdx << " = sub i32 " << sp << ", 1\n";
            std::string topPtr = nextReg();
            out_ << "  " << topPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << topIdx << "\n";
            out_ << "  call void @rq_not(ptr sret(%struct.RqValue) align 8 %_tmp, ptr " << topPtr << ")\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << topPtr << ", ptr align 8 %_tmp, i64 16, i1 false)\n";
            break;
        }
        case OpCode::Return: {
            if (fn.name.empty()) {
                out_ << "  ret void\n";
            } else {
                popStackToPtr("%_ret");
                out_ << "  ret void\n";
            }
            break;
        }
        case OpCode::Jump:
        case OpCode::Loop: {
            out_ << "  br label %bb_" << ins.a << "\n";
            break;
        }
        case OpCode::JumpIfFalse: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string prevSp = nextReg();
            out_ << "  " << prevSp << " = sub i32 " << sp << ", 1\n";
            out_ << "  store i32 " << prevSp << ", ptr %_sp, align 4\n";
            std::string topPtr = nextReg();
            out_ << "  " << topPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << prevSp << "\n";
            std::string cond = nextReg();
            out_ << "  " << cond << " = call i32 @rq_truthy(ptr " << topPtr << ")\n";
            std::string boolCond = nextReg();
            out_ << "  " << boolCond << " = icmp ne i32 " << cond << ", 0\n";
            out_ << "  br i1 " << boolCond << ", label %bb_" << (ip + 1) << ", label %bb_" << ins.a << "\n";
            break;
        }
        case OpCode::Call: {
            const auto& cval = fn.constants.at(static_cast<size_t>(ins.a));
            const std::string& callee = std::get<std::string>(cval.data);
            int argc = ins.b;

            if (callee.find('.') != std::string::npos || callee == "print" || callee == "println" || callee == "len" || callee == "assert" || callee == "assert_eq" || callee == "clock" || callee == "floor" || callee == "ceil" || callee == "round" || callee == "abs" || callee == "sqrt" || callee == "sin" || callee == "cos" || callee == "tan" || callee == "int" || callee == "float" || callee == "str" || callee == "bool") {
                std::string strId = getStringConstant(callee);
                std::string sp = nextReg();
                out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
                std::string baseSp = nextReg();
                out_ << "  " << baseSp << " = sub i32 " << sp << ", " << argc << "\n";
                out_ << "  store i32 " << baseSp << ", ptr %_sp, align 4\n";
                std::string argsPtr = nextReg();
                out_ << "  " << argsPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << baseSp << "\n";
                out_ << "  call void @rq_call_builtin(ptr sret(%struct.RqValue) align 8 %_tmp, ptr @" << strId << ", i32 " << argc << ", ptr " << argsPtr << ")\n";
                pushStackFromPtr("%_tmp");
            } else {
                std::string sp = nextReg();
                out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
                std::string baseSp = nextReg();
                out_ << "  " << baseSp << " = sub i32 " << sp << ", " << argc << "\n";
                out_ << "  store i32 " << baseSp << ", ptr %_sp, align 4\n";

                std::string mangled = "_rq_fn_" + safeName(callee);
                out_ << "  call void @" << mangled << "(ptr sret(%struct.RqValue) align 8 %_tmp";
                for (int i = 0; i < argc; ++i) {
                    std::string argPtr = nextReg();
                    out_ << "  " << argPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << (baseSp + "+" + std::to_string(i)) << "\n";
                    out_ << ", ptr " << argPtr;
                }
                out_ << ")\n";
                pushStackFromPtr("%_tmp");
            }
            break;
        }
        case OpCode::BuildArray: {
            int cnt = ins.a;
            out_ << "  call void @rq_arr_new(ptr sret(%struct.RqValue) align 8 %_tmp, i32 " << std::max(cnt, 8) << ")\n";
            std::string arrObj = nextReg();
            out_ << "  " << arrObj << " = alloca %struct.RqValue, align 8\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << arrObj << ", ptr align 8 %_tmp, i64 16, i1 false)\n";

            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string baseSp = nextReg();
            out_ << "  " << baseSp << " = sub i32 " << sp << ", " << cnt << "\n";
            out_ << "  store i32 " << baseSp << ", ptr %_sp, align 4\n";

            for (int i = 0; i < cnt; ++i) {
                std::string itemPtr = nextReg();
                out_ << "  " << itemPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << (baseSp + "+" + std::to_string(i)) << "\n";
                out_ << "  call void @rq_push(ptr " << arrObj << ", ptr " << itemPtr << ")\n";
            }
            pushStackFromPtr(arrObj);
            break;
        }
        case OpCode::BuildObject: {
            int cnt = ins.a;
            out_ << "  call void @rq_obj_new(ptr sret(%struct.RqValue) align 8 %_tmp, i32 " << std::max(cnt, 8) << ")\n";
            std::string mapObj = nextReg();
            out_ << "  " << mapObj << " = alloca %struct.RqValue, align 8\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << mapObj << ", ptr align 8 %_tmp, i64 16, i1 false)\n";

            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string baseSp = nextReg();
            out_ << "  " << baseSp << " = sub i32 " << sp << ", " << (cnt * 2) << "\n";
            out_ << "  store i32 " << baseSp << ", ptr %_sp, align 4\n";

            for (int i = 0; i < cnt; ++i) {
                std::string kPtr = nextReg();
                out_ << "  " << kPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << (baseSp + "+" + std::to_string(i * 2)) << "\n";
                std::string vPtr = nextReg();
                out_ << "  " << vPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << (baseSp + "+" + std::to_string(i * 2 + 1)) << "\n";
                out_ << "  call void @rq_set_prop(ptr " << mapObj << ", ptr " << kPtr << ", ptr " << vPtr << ")\n";
            }
            pushStackFromPtr(mapObj);
            break;
        }
        case OpCode::GetIndex: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string idxSp = nextReg();
            out_ << "  " << idxSp << " = sub i32 " << sp << ", 1\n";
            std::string objSp = nextReg();
            out_ << "  " << objSp << " = sub i32 " << sp << ", 2\n";
            std::string idxPtr = nextReg();
            out_ << "  " << idxPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << idxSp << "\n";
            std::string objPtr = nextReg();
            out_ << "  " << objPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << objSp << "\n";
            out_ << "  call void @rq_get_index(ptr sret(%struct.RqValue) align 8 %_tmp, ptr " << objPtr << ", ptr " << idxPtr << ")\n";
            out_ << "  call void @llvm.memcpy.p0.p0.i64(ptr align 8 " << objPtr << ", ptr align 8 %_tmp, i64 16, i1 false)\n";
            std::string nextSp = nextReg();
            out_ << "  " << nextSp << " = sub i32 " << sp << ", 1\n";
            out_ << "  store i32 " << nextSp << ", ptr %_sp, align 4\n";
            break;
        }
        case OpCode::SetIndex: {
            std::string sp = nextReg();
            out_ << "  " << sp << " = load i32, ptr %_sp, align 4\n";
            std::string valSp = nextReg();
            out_ << "  " << valSp << " = sub i32 " << sp << ", 1\n";
            std::string idxSp = nextReg();
            out_ << "  " << idxSp << " = sub i32 " << sp << ", 2\n";
            std::string objSp = nextReg();
            out_ << "  " << objSp << " = sub i32 " << sp << ", 3\n";
            std::string valPtr = nextReg();
            out_ << "  " << valPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << valSp << "\n";
            std::string idxPtr = nextReg();
            out_ << "  " << idxPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << idxSp << "\n";
            std::string objPtr = nextReg();
            out_ << "  " << objPtr << " = getelementptr inbounds [256 x %struct.RqValue], ptr %_stack, i64 0, i32 " << objSp << "\n";
            out_ << "  call void @rq_set_index(ptr " << objPtr << ", ptr " << idxPtr << ", ptr " << valPtr << ")\n";
            std::string nextSp = nextReg();
            out_ << "  " << nextSp << " = sub i32 " << sp << ", 3\n";
            out_ << "  store i32 " << nextSp << ", ptr %_sp, align 4\n";
            break;
        }
        default:
            break;
        }
    }

    void emitProgram(const BytecodeProgram& program) {
        emitHeader();

        collectGlobals(program.entry);
        for (const auto& [name, fn] : program.functions) {
            collectGlobals(fn);
        }

        for (const auto& [name, fn] : program.functions) {
            emitFunctionSignature(fn, true);
            out_ << "\n";
        }
        out_ << "\n";

        for (const auto& [name, fn] : program.functions) {
            emitFunction(fn);
        }

        emitFunction(program.entry);

        const std::string entryFname = program.entry.name.empty() ? "_rq_main" : "_rq_fn_" + safeName(program.entry.name);
        out_ << "define i32 @main(i32 %argc, ptr %argv) {\n";
        out_ << "entry:\n";
        out_ << "  call void @rq_runtime_init()\n";
        out_ << "  call void @rq_num(ptr sret(%struct.RqValue) align 8 @_g_pi, double 0x400921fb54442d18)\n";
        out_ << "  call void @" << entryFname << "()\n";
        out_ << "  ret i32 0\n";
        out_ << "}\n\n";

        emitStringConstants();
    }
};
