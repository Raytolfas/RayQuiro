#pragma once
// CEmitter.h - Translates BytecodeProgram to C source for native compilation
// rqio build script.rq -o app  ->  native binary via clang/gcc

#include "Bytecode.h"
#include <sstream>
#include <string>
#include <unordered_set>
#include <variant>
#include <algorithm>
#include <cmath>

class CEmitter {
public:
    static std::string emit(const BytecodeProgram& program) {
        CEmitter e;
        e.emitProgram(program);
        return e.out_.str();
    }

private:
    std::ostringstream out_;
    std::unordered_set<std::string> globals_declared_;

    static std::string safeName(const std::string& n) {
        std::string r;
        for (char c : n)
            r += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
        if (!r.empty() && std::isdigit((unsigned char)r[0])) r = "_" + r;
        return r;
    }

    static std::string escStr(const std::string& s) {
        std::string r = "\"";
        for (unsigned char c : s) {
            if      (c == '\\') r += "\\\\";
            else if (c == '"')  r += "\\\"";
            else if (c == '\n') r += "\\n";
            else if (c == '\r') r += "\\r";
            else if (c == '\t') r += "\\t";
            else if (c < 32)  { char buf[8]; snprintf(buf,sizeof(buf),"\\x%02x",c); r+=buf; }
            else r += (char)c;
        }
        return r + '"';
    }

    static std::string vmvalToC(const VMValue& v) {
        if (std::holds_alternative<std::monostate>(v.data)) return "rq_null()";
        if (auto* d = std::get_if<double>(&v.data)) {
            double n = *d;
            if (std::isinf(n) || std::isnan(n)) return "rq_num(0.0)";
            if (n == (long long)n && n >= -1e15 && n <= 1e15)
                return "rq_num(" + std::to_string((long long)n) + ".0)";
            char buf[64]; snprintf(buf,sizeof(buf),"rq_num(%.17g)",n); return buf;
        }
        if (auto* s = std::get_if<std::string>(&v.data))
            return "rq_str_copy(" + escStr(*s) + ")";
        if (auto* b = std::get_if<bool>(&v.data))
            return *b ? "rq_bool(1)" : "rq_bool(0)";
        return "rq_null()";
    }

    void collectGlobals(const BytecodeFunction& fn) {
        for (const auto& ins : fn.code) {
            auto op = ins.op;
            bool isGlobal = (op == OpCode::DefineGlobal || op == OpCode::GetGlobal ||
                             op == OpCode::SetGlobal     || op == OpCode::SetGlobalIndex ||
                             op == OpCode::AppendGlobal  || op == OpCode::AppendObjGlobal);
            if (!isGlobal) continue;
            const auto& cval = fn.constants.at((size_t)ins.a);
            if (!std::holds_alternative<std::string>(cval.data)) continue;
            const std::string& name = std::get<std::string>(cval.data);
            if (globals_declared_.insert(name).second)
                out_ << "static RqValue _g_" << safeName(name) << ";\n";
        }
    }

    void emitFunction(const BytecodeFunction& fn) {
        const std::string fname = fn.name.empty()
            ? "_rq_main" : "_rq_fn_" + safeName(fn.name);

        if (fn.name.empty()) {
            out_ << "void " << fname << "(void) {\n";
        } else {
            out_ << "RqValue " << fname << "(";
            for (size_t i = 0; i < fn.params.size(); ++i) {
                if (i) out_ << ", ";
                out_ << "RqValue _p" << i;
            }
            if (fn.params.empty()) out_ << "void";
            out_ << ") {\n";
        }

        int nlocals = std::max(fn.localCount, (int)fn.params.size()) + 8;
        out_ << "    RqValue _locals[" << nlocals << "];\n";
        out_ << "    for(int _li=0;_li<" << nlocals << ";_li++) _locals[_li]=rq_null();\n";
        for (size_t i = 0; i < fn.params.size(); ++i)
            out_ << "    _locals[" << i << "]=_p" << i << ";\n";
        out_ << "    RqValue _stack[256]; int _sp=0; (void)_sp;\n";

        // collect jump targets
        std::unordered_set<int> labels;
        for (const auto& ins : fn.code) {
            if (ins.op==OpCode::Jump||ins.op==OpCode::JumpIfFalse||ins.op==OpCode::Loop)
                labels.insert(ins.a);
            if (ins.op==OpCode::JumpIfNotNull) labels.insert(ins.b);
            if (ins.op==OpCode::TryBegin) labels.insert(ins.a);
            if (ins.op==OpCode::TryEnd)   labels.insert(ins.a);
        }

        for (size_t ip=0; ip<fn.code.size(); ++ip) {
            if (labels.count((int)ip)) out_ << "_L" << ip << ":;\n";
            emitInstr(fn, ip);
        }
        if (labels.count((int)fn.code.size()))
            out_ << "_L" << fn.code.size() << ":;\n";

        if (fn.name.empty()) out_ << "}\n";
        else out_ << "    return rq_null();\n}\n";
    }

    void emitInstr(const BytecodeFunction& fn, size_t ip) {
        const Instruction& ins = fn.code[ip];
        out_ << "    /*" << ip << "*/ ";

        auto cname = [&](int idx) -> std::string {
            return std::get<std::string>(fn.constants.at((size_t)idx).data);
        };

        switch (ins.op) {
        case OpCode::Constant:
            out_ << "_stack[_sp++]=" << vmvalToC(fn.constants.at((size_t)ins.a)) << ";\n"; break;
        case OpCode::Null:  out_ << "_stack[_sp++]=rq_null();\n"; break;
        case OpCode::True:  out_ << "_stack[_sp++]=rq_bool(1);\n"; break;
        case OpCode::False: out_ << "_stack[_sp++]=rq_bool(0);\n"; break;
        case OpCode::Pop:   out_ << "_sp--;\n"; break;
        case OpCode::Dup:   out_ << "_stack[_sp]=_stack[_sp-1];_sp++;\n"; break;

        case OpCode::DefineGlobal:
            out_ << "_g_" << safeName(cname(ins.a)) << "=_stack[--_sp];\n"; break;
        case OpCode::SetGlobal:
            out_ << "_g_" << safeName(cname(ins.a)) << "=_stack[_sp-1];\n"; break;
        case OpCode::GetGlobal:
            out_ << "_stack[_sp++]=_g_" << safeName(cname(ins.a)) << ";\n"; break;
        case OpCode::GetLocal:
            out_ << "_stack[_sp++]=_locals[" << ins.a << "];\n"; break;
        case OpCode::SetLocal:
            out_ << "_locals[" << ins.a << "]=_stack[_sp-1];\n"; break;

        case OpCode::Add:      out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_add(_a,_b);}\n"; break;
        case OpCode::Subtract: out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_sub(_a,_b);}\n"; break;
        case OpCode::Multiply: out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_mul(_a,_b);}\n"; break;
        case OpCode::Divide:   out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_div(_a,_b);}\n"; break;
        case OpCode::Modulo:   out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_mod(_a,_b);}\n"; break;
        case OpCode::Negate:   out_ << "_stack[_sp-1]=rq_neg(_stack[_sp-1]);\n"; break;
        case OpCode::Not:      out_ << "_stack[_sp-1]=rq_not(_stack[_sp-1]);\n"; break;

        case OpCode::Equal:        out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_eq(_a,_b);}\n"; break;
        case OpCode::NotEqual:     out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_ne(_a,_b);}\n"; break;
        case OpCode::Greater:      out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_gt(_a,_b);}\n"; break;
        case OpCode::GreaterEqual: out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_ge(_a,_b);}\n"; break;
        case OpCode::Less:         out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_lt(_a,_b);}\n"; break;
        case OpCode::LessEqual:    out_ << "{RqValue _b=_stack[--_sp],_a=_stack[--_sp];_stack[_sp++]=rq_le(_a,_b);}\n"; break;

        case OpCode::Jump:         out_ << "goto _L" << ins.a << ";\n"; break;
        case OpCode::JumpIfFalse:  out_ << "if(!rq_truthy(_stack[_sp-1]))goto _L" << ins.a << ";\n"; break;
        case OpCode::Loop:         out_ << "goto _L" << ins.a << ";\n"; break;
        case OpCode::JumpIfNotNull: out_ << "if(_stack[_sp-1].type!=RQ_NULL)goto _L" << ins.b << ";\n"; break;

        case OpCode::Return:
            if (fn.name.empty()) out_ << "return;\n";
            else out_ << "{RqValue _ret=(_sp>0)?_stack[--_sp]:rq_null();return _ret;}\n";
            break;

        case OpCode::Throw:
            out_ << "{char* _msg=rq_to_cstr(_stack[--_sp]);fprintf(stderr,\"[RayQuiro] Error: %s\\n\",_msg);free(_msg);exit(1);}\n"; break;

        case OpCode::Concat: {
            int n = ins.a;
            out_ << "{RqValue _pts[" << n << "];for(int _ci=" << (n-1) << ";_ci>=0;_ci--)_pts[_ci]=_stack[--_sp];\n"
                 << "  size_t _tl=0;char* _pp[" << n << "];for(int _ci=0;_ci<" << n << ";_ci++){_pp[_ci]=rq_to_cstr(_pts[_ci]);_tl+=strlen(_pp[_ci]);}\n"
                 << "  char* _cr=(char*)malloc(_tl+1);_cr[0]=0;for(int _ci=0;_ci<" << n << ";_ci++){strcat(_cr,_pp[_ci]);free(_pp[_ci]);}\n"
                 << "  RqValue _rs;_rs.type=RQ_STR;_rs.d.str=_cr;_stack[_sp++]=_rs;}\n";
            break;
        }

        case OpCode::TryBegin: out_ << "/* TryBegin: catch=L" << ins.a << " */\n"; break;
        case OpCode::TryEnd:   out_ << "goto _L" << ins.a << "; /* TryEnd */\n"; break;

        case OpCode::Call: {
            std::string callee = cname(ins.a);
            int argc = ins.b;
            out_ << "{RqValue _ca[" << std::max(argc,1) << "];";
            out_ << "for(int _ai=" << (argc-1) << ";_ai>=0;_ai--)_ca[_ai]=_stack[--_sp];";
            out_ << "_stack[_sp++]=" << emitCallExpr(callee, argc) << ";}\n";
            break;
        }

        // MVP: unsupported opcodes are skipped gracefully
        case OpCode::BuildArray:
        case OpCode::BuildObject:
            out_ << "/* " << (int)(uint8_t)ins.op << ": BuildArr/Obj MVP skip */ _stack[_sp++]=rq_null();\n"; break;
        case OpCode::GetIndex:
            out_ << "/* GetIndex MVP */ _sp-=2; _stack[_sp++]=rq_null();\n"; break;
        default:
            out_ << "/* opcode " << (int)(uint8_t)ins.op << " MVP skip */ if(_sp>0)_sp--;\n"; break;
        }
    }

    std::string emitCallExpr(const std::string& callee, int argc) {
        if (callee=="print")       return "rq_print(_ca[0])";
        if (callee=="str")         return "rq_str_builtin(_ca[0])";
        if (callee=="num")         return "rq_num_builtin(_ca[0])";
        if (callee=="bool")        return "rq_bool_builtin(_ca[0])";
        if (callee=="type")        return "rq_type_builtin(_ca[0])";
        if (callee=="math.sqrt")   return "rq_sqrt(_ca[0])";
        if (callee=="math.abs")    return "rq_abs(_ca[0])";
        if (callee=="math.floor")  return "rq_floor(_ca[0])";
        if (callee=="math.ceil")   return "rq_ceil(_ca[0])";
        if (callee=="math.round")  return "rq_round(_ca[0])";
        if (callee=="math.pow")    return "rq_pow(_ca[0],_ca[1])";
        // user-defined function call
        std::string r = "_rq_fn_" + safeName(callee) + "(";
        for (int i=0; i<argc; i++) { if(i) r+=","; r+="_ca["+std::to_string(i)+"]"; }
        r += ")";
        return r;
    }

    void emitProgram(const BytecodeProgram& program) {
        out_ << "// Generated by rqio build — RayQuiro C Backend\n"
             << "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <math.h>\n"
             << "#include \"rq_runtime.h\"\n\n";

        // Forward-declare user functions
        for (const auto& [name, fn] : program.functions) {
            out_ << "RqValue _rq_fn_" << safeName(name) << "(";
            for (size_t i=0; i<fn.params.size(); ++i) { if(i) out_<<","; out_<<"RqValue _p"<<i; }
            if (fn.params.empty()) out_ << "void";
            out_ << ");\n";
        }
        // Forward-declare entry function
        {
            const std::string entryFname = program.entry.name.empty()
                ? "_rq_main" : "_rq_fn_" + safeName(program.entry.name);
            const bool entryIsVoid = program.entry.name.empty();
            out_ << (entryIsVoid ? "void " : "RqValue ") << entryFname << "(void);\n";
        }
        out_ << "\n";

        // Collect & declare globals
        collectGlobals(program.entry);
        for (const auto& [name, fn] : program.functions) collectGlobals(fn);
        out_ << "\n";

        // Emit user-defined functions
        for (const auto& [name, fn] : program.functions) {
            emitFunction(fn); out_ << "\n";
        }

        // Emit entry point
        emitFunction(program.entry);

        // Determine actual entry function name
        const std::string entryFname = program.entry.name.empty()
            ? "_rq_main" : "_rq_fn_" + safeName(program.entry.name);

        // main()
        out_ << "\nint main(int argc, char** argv) {\n"
             << "    (void)argc; (void)argv;\n"
             << "    " << entryFname << "();\n    return 0;\n}\n";
    }
};
