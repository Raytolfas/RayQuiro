#pragma once

#include "Bytecode.h"
#include <sstream>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <vector>

class LLVMEmitter {
public:
    static std::string emit(const BytecodeProgram& program, const std::string& targetTriple = "") {
        LLVMEmitter e;
        e.targetTriple_ = targetTriple.empty() ? defaultTriple() : targetTriple;
        e.emitModule(program);
        return e.out_.str();
    }

private:
    std::ostringstream out_;
    std::ostringstream strBuf_;
    std::unordered_set<std::string> globals_declared_;
    int strConst_ = 0;
    std::string targetTriple_;

    static std::string defaultTriple() {
#if defined(_WIN32) && defined(__x86_64__)
        return "x86_64-pc-windows-gnu";
#elif defined(_WIN32)
        return "i686-pc-windows-gnu";
#elif defined(__APPLE__)
        return "x86_64-apple-macosx10.15.0";
#elif defined(__aarch64__)
        return "aarch64-unknown-linux-gnu";
#else
        return "x86_64-unknown-linux-gnu";
#endif
    }

    static std::string safeName(const std::string& n) {
        std::string r;
        for (char c : n)
            r += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
        if (!r.empty() && std::isdigit((unsigned char)r[0])) r = "_" + r;
        return r;
    }

    static std::string escLLVM(const std::string& s) {
        std::string r;
        for (unsigned char c : s) {
            if      (c == '\\') r += "\\5C";
            else if (c == '"')  r += "\\22";
            else if (c == '\n') r += "\\0A";
            else if (c == '\r') r += "\\0D";
            else if (c == '\t') r += "\\09";
            else if (c < 32)  { char buf[8]; snprintf(buf,sizeof(buf),"\\%02X",c); r+=buf; }
            else r += (char)c;
        }
        return r;
    }

    void emitRuntimeDecls() {
        out_ << "%RqValue = type { i32, double }\n\n";

        static const char* decls[] = {
            "declare %RqValue @rq_null()",
            "declare %RqValue @rq_num(double)",
            "declare %RqValue @rq_bool(i32)",
            "declare %RqValue @rq_str_copy(i8*)",
            "declare i32 @rq_truthy(%RqValue)",
            "declare %RqValue @rq_add(%RqValue, %RqValue)",
            "declare %RqValue @rq_sub(%RqValue, %RqValue)",
            "declare %RqValue @rq_mul(%RqValue, %RqValue)",
            "declare %RqValue @rq_div(%RqValue, %RqValue)",
            "declare %RqValue @rq_mod(%RqValue, %RqValue)",
            "declare %RqValue @rq_neg(%RqValue)",
            "declare %RqValue @rq_not(%RqValue)",
            "declare %RqValue @rq_eq(%RqValue, %RqValue)",
            "declare %RqValue @rq_ne(%RqValue, %RqValue)",
            "declare %RqValue @rq_lt(%RqValue, %RqValue)",
            "declare %RqValue @rq_le(%RqValue, %RqValue)",
            "declare %RqValue @rq_gt(%RqValue, %RqValue)",
            "declare %RqValue @rq_ge(%RqValue, %RqValue)",
            "declare %RqValue @rq_print(%RqValue)",
            "declare %RqValue @rq_str_builtin(%RqValue)",
            "declare %RqValue @rq_num_builtin(%RqValue)",
            "declare %RqValue @rq_bool_builtin(%RqValue)",
            "declare %RqValue @rq_type_builtin(%RqValue)",
            "declare %RqValue @rq_sqrt(%RqValue)",
            "declare %RqValue @rq_abs(%RqValue)",
            "declare %RqValue @rq_floor(%RqValue)",
            "declare %RqValue @rq_ceil(%RqValue)",
            "declare %RqValue @rq_round(%RqValue)",
            "declare %RqValue @rq_pow(%RqValue, %RqValue)",
            "declare %RqValue @rq_concat_n(%RqValue*, i32)",
            "declare i8* @rq_to_cstr(%RqValue)",
            "declare void @rq_throw_msg(i8*)",
            nullptr
        };
        for (int i = 0; decls[i]; ++i) out_ << decls[i] << "\n";
        out_ << "\n";
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
                out_ << "@_g_" << safeName(name) << " = global %RqValue zeroinitializer\n";
        }
    }

    struct FnCtx {
        int reg = 0;
        std::string spVar;
        std::string stackVar;
        std::string localsVar;

        std::string R() { return "%r" + std::to_string(reg++); }
    };

    void push(FnCtx& ctx, const std::string& val, std::ostringstream& o) {
        auto sp  = ctx.R(); auto ptr = ctx.R(); auto sp1 = ctx.R();
        o << "  " << sp  << " = load i32, i32* " << ctx.spVar << "\n";
        o << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.stackVar << ", i32 " << sp << "\n";
        o << "  store %RqValue " << val << ", %RqValue* " << ptr << "\n";
        o << "  " << sp1 << " = add i32 " << sp << ", 1\n";
        o << "  store i32 " << sp1 << ", i32* " << ctx.spVar << "\n";
    }

    std::string pop(FnCtx& ctx, std::ostringstream& o) {
        auto sp1=ctx.R(); auto sp0=ctx.R(); auto ptr=ctx.R(); auto val=ctx.R();
        o << "  " << sp1 << " = load i32, i32* " << ctx.spVar << "\n";
        o << "  " << sp0 << " = sub i32 " << sp1 << ", 1\n";
        o << "  store i32 " << sp0 << ", i32* " << ctx.spVar << "\n";
        o << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.stackVar << ", i32 " << sp0 << "\n";
        o << "  " << val << " = load %RqValue, %RqValue* " << ptr << "\n";
        return val;
    }

    std::string peek(FnCtx& ctx, std::ostringstream& o) {
        auto sp1=ctx.R(); auto sp0=ctx.R(); auto ptr=ctx.R(); auto val=ctx.R();
        o << "  " << sp1 << " = load i32, i32* " << ctx.spVar << "\n";
        o << "  " << sp0 << " = sub i32 " << sp1 << ", 1\n";
        o << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.stackVar << ", i32 " << sp0 << "\n";
        o << "  " << val << " = load %RqValue, %RqValue* " << ptr << "\n";
        return val;
    }

    std::string getLocal(FnCtx& ctx, std::ostringstream& o, int slot) {
        auto ptr=ctx.R(); auto val=ctx.R();
        o << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.localsVar << ", i32 " << slot << "\n";
        o << "  " << val << " = load %RqValue, %RqValue* " << ptr << "\n";
        return val;
    }

    void setLocal(FnCtx& ctx, std::ostringstream& o, int slot, const std::string& val) {
        auto ptr=ctx.R();
        o << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.localsVar << ", i32 " << slot << "\n";
        o << "  store %RqValue " << val << ", %RqValue* " << ptr << "\n";
    }

    std::string getGlobal(FnCtx& ctx, std::ostringstream& o, const std::string& name) {
        auto val=ctx.R();
        o << "  " << val << " = load %RqValue, %RqValue* @_g_" << safeName(name) << "\n";
        return val;
    }

    void setGlobal(std::ostringstream& o, const std::string& name, const std::string& val) {
        o << "  store %RqValue " << val << ", %RqValue* @_g_" << safeName(name) << "\n";
    }

    std::string rt0(FnCtx& ctx, std::ostringstream& o, const char* fn) {
        auto r=ctx.R(); o << "  " << r << " = call %RqValue @" << fn << "()\n"; return r;
    }

    std::string rt1(FnCtx& ctx, std::ostringstream& o, const char* fn, const std::string& a) {
        auto r=ctx.R(); o << "  " << r << " = call %RqValue @" << fn << "(%RqValue " << a << ")\n"; return r;
    }

    std::string rt2(FnCtx& ctx, std::ostringstream& o, const char* fn, const std::string& a, const std::string& b) {
        auto r=ctx.R(); o << "  " << r << " = call %RqValue @" << fn << "(%RqValue " << a << ", %RqValue " << b << ")\n"; return r;
    }

    std::string makeStrConst(const std::string& s) {
        std::string id = "@.str" + std::to_string(strConst_++);
        size_t len = s.size() + 1;
        strBuf_ << id << " = private unnamed_addr constant [" << len << " x i8] c\""
                << escLLVM(s) << "\\00\"\n";
        return id;
    }

    std::string emitCallExpr(FnCtx& ctx, std::ostringstream& o, const std::string& callee,
                             const std::vector<std::string>& args, const BytecodeProgram& program) {
        auto a0 = [&]() { return args.size()>0 ? args[0] : rt0(ctx,o,"rq_null"); };
        auto a1 = [&]() { return args.size()>1 ? args[1] : rt0(ctx,o,"rq_null"); };

        if (callee=="print")       return rt1(ctx,o,"rq_print",a0());
        if (callee=="str")         return rt1(ctx,o,"rq_str_builtin",a0());
        if (callee=="num")         return rt1(ctx,o,"rq_num_builtin",a0());
        if (callee=="bool")        return rt1(ctx,o,"rq_bool_builtin",a0());
        if (callee=="type")        return rt1(ctx,o,"rq_type_builtin",a0());
        if (callee=="math.sqrt")   return rt1(ctx,o,"rq_sqrt",a0());
        if (callee=="math.abs")    return rt1(ctx,o,"rq_abs",a0());
        if (callee=="math.floor")  return rt1(ctx,o,"rq_floor",a0());
        if (callee=="math.ceil")   return rt1(ctx,o,"rq_ceil",a0());
        if (callee=="math.round")  return rt1(ctx,o,"rq_round",a0());
        if (callee=="math.pow")    return rt2(ctx,o,"rq_pow",a0(),a1());

        auto r = ctx.R();
        o << "  " << r << " = call %RqValue @_rq_fn_" << safeName(callee) << "(";
        for (size_t i = 0; i < args.size(); ++i) { if(i) o << ", "; o << "%RqValue " << args[i]; }
        o << ")\n";
        return r;
    }

    void emitFunction(const BytecodeProgram& program, const BytecodeFunction& fn, bool isEntry = false) {
        const std::string fname = isEntry ? "_rq_main" : "_rq_fn_" + safeName(fn.name);

        std::ostringstream body;
        FnCtx ctx;
        ctx.stackVar  = "%_stk";
        ctx.spVar     = "%_sp";
        ctx.localsVar = "%_loc";

        int nlocals = std::max(fn.localCount, (int)fn.params.size()) + 8;
        body << "  " << ctx.stackVar  << " = alloca %RqValue, i32 256\n";
        body << "  " << ctx.spVar     << " = alloca i32\n";
        body << "  store i32 0, i32* " << ctx.spVar << "\n";
        body << "  " << ctx.localsVar << " = alloca %RqValue, i32 " << nlocals << "\n";

        {
            auto nv = rt0(ctx, body, "rq_null");
            for (int i = 0; i < nlocals; ++i) {
                auto ptr = ctx.R();
                body << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.localsVar << ", i32 " << i << "\n";
                body << "  store %RqValue " << nv << ", %RqValue* " << ptr << "\n";
            }
        }
        for (size_t i = 0; i < fn.params.size(); ++i) {
            auto ptr = ctx.R();
            body << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << ctx.localsVar << ", i32 " << i << "\n";
            body << "  store %RqValue %p" << i << ", %RqValue* " << ptr << "\n";
        }

        std::unordered_set<int> labels;
        for (size_t i = 0; i < fn.code.size(); ++i) {
            const auto& ins = fn.code[i];
            if (ins.op==OpCode::Jump||ins.op==OpCode::JumpIfFalse||ins.op==OpCode::Loop)
                labels.insert(ins.a);
            if (ins.op==OpCode::JumpIfNotNull) labels.insert(ins.b);
            if (ins.op==OpCode::TryBegin) labels.insert(ins.a);
            if (ins.op==OpCode::TryEnd)   labels.insert(ins.a);
            labels.insert((int)(i+1));
        }

        body << "  br label %_ip0\n";

        for (size_t ip = 0; ip < fn.code.size(); ++ip) {
            body << "_ip" << ip << ":\n";
            emitInstr(program, fn, ctx, body, ip, labels, isEntry);
        }
        body << "_ip" << fn.code.size() << ":\n";
        if (isEntry) body << "  ret void\n";
        else {
            auto nv = rt0(ctx, body, "rq_null");
            body << "  ret %RqValue " << nv << "\n";
        }

        if (isEntry) {
            out_ << "define void @" << fname << "() {\nentry:\n";
        } else {
            out_ << "define %RqValue @" << fname << "(";
            for (size_t i = 0; i < fn.params.size(); ++i) {
                if(i) out_ << ", ";
                out_ << "%RqValue %p" << i;
            }
            out_ << ") {\nentry:\n";
        }
        out_ << body.str() << "}\n\n";
    }

    void emitInstr(const BytecodeProgram& program, const BytecodeFunction& fn,
                   FnCtx& ctx, std::ostringstream& o, size_t ip,
                   const std::unordered_set<int>& labels, bool isEntry) {
        const Instruction& ins = fn.code[ip];

        auto cname = [&](int idx) -> std::string {
            return std::get<std::string>(fn.constants.at((size_t)idx).data);
        };
        auto fallthrough = [&]() {
            o << "  br label %_ip" << (ip+1) << "\n";
        };

        switch (ins.op) {
        case OpCode::Constant: {
            const VMValue& cv = fn.constants.at((size_t)ins.a);
            std::string val;
            if (std::holds_alternative<std::monostate>(cv.data)) {
                val = rt0(ctx,o,"rq_null");
            } else if (auto* d = std::get_if<double>(&cv.data)) {
                if (std::isinf(*d)||std::isnan(*d)) { val=rt0(ctx,o,"rq_null"); }
                else {
                    auto r=ctx.R(); char buf[64]; snprintf(buf,sizeof(buf),"%.17e",*d);
                    o << "  " << r << " = call %RqValue @rq_num(double " << buf << ")\n";
                    val=r;
                }
            } else if (auto* s = std::get_if<std::string>(&cv.data)) {
                std::string id = makeStrConst(*s);
                size_t len = s->size()+1;
                auto gep=ctx.R(); auto r=ctx.R();
                o << "  " << gep << " = getelementptr [" << len << " x i8], [" << len << " x i8]* "
                  << id << ", i32 0, i32 0\n";
                o << "  " << r << " = call %RqValue @rq_str_copy(i8* " << gep << ")\n";
                val=r;
            } else if (auto* b = std::get_if<bool>(&cv.data)) {
                auto r=ctx.R();
                o << "  " << r << " = call %RqValue @rq_bool(i32 " << (*b?1:0) << ")\n";
                val=r;
            } else { val=rt0(ctx,o,"rq_null"); }
            push(ctx,val,o); fallthrough(); break;
        }
        case OpCode::Null:  { push(ctx,rt0(ctx,o,"rq_null"),o); fallthrough(); break; }
        case OpCode::True:  { auto r=ctx.R(); o<<"  "<<r<<" = call %RqValue @rq_bool(i32 1)\n"; push(ctx,r,o); fallthrough(); break; }
        case OpCode::False: { auto r=ctx.R(); o<<"  "<<r<<" = call %RqValue @rq_bool(i32 0)\n"; push(ctx,r,o); fallthrough(); break; }
        case OpCode::Pop:   { pop(ctx,o); fallthrough(); break; }
        case OpCode::Dup:   { push(ctx,peek(ctx,o),o); fallthrough(); break; }

        case OpCode::DefineGlobal: { auto v=pop(ctx,o); setGlobal(o,cname(ins.a),v); fallthrough(); break; }
        case OpCode::SetGlobal:    { setGlobal(o,cname(ins.a),peek(ctx,o)); fallthrough(); break; }
        case OpCode::GetGlobal:    { push(ctx,getGlobal(ctx,o,cname(ins.a)),o); fallthrough(); break; }
        case OpCode::GetLocal:     { push(ctx,getLocal(ctx,o,ins.a),o); fallthrough(); break; }
        case OpCode::SetLocal:     { setLocal(ctx,o,ins.a,peek(ctx,o)); fallthrough(); break; }

        case OpCode::Add:          { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_add",a,b),o); fallthrough(); break; }
        case OpCode::Subtract:     { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_sub",a,b),o); fallthrough(); break; }
        case OpCode::Multiply:     { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_mul",a,b),o); fallthrough(); break; }
        case OpCode::Divide:       { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_div",a,b),o); fallthrough(); break; }
        case OpCode::Modulo:       { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_mod",a,b),o); fallthrough(); break; }
        case OpCode::Negate:       { auto v=pop(ctx,o); push(ctx,rt1(ctx,o,"rq_neg",v),o); fallthrough(); break; }
        case OpCode::Not:          { auto v=pop(ctx,o); push(ctx,rt1(ctx,o,"rq_not",v),o); fallthrough(); break; }
        case OpCode::Equal:        { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_eq",a,b),o); fallthrough(); break; }
        case OpCode::NotEqual:     { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_ne",a,b),o); fallthrough(); break; }
        case OpCode::Greater:      { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_gt",a,b),o); fallthrough(); break; }
        case OpCode::GreaterEqual: { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_ge",a,b),o); fallthrough(); break; }
        case OpCode::Less:         { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_lt",a,b),o); fallthrough(); break; }
        case OpCode::LessEqual:    { auto b=pop(ctx,o),a=pop(ctx,o); push(ctx,rt2(ctx,o,"rq_le",a,b),o); fallthrough(); break; }

        case OpCode::Jump:
        case OpCode::Loop:
            o << "  br label %_ip" << ins.a << "\n";
            break;

        case OpCode::JumpIfFalse: {
            auto v=peek(ctx,o); auto tr=ctx.R(); auto cmp=ctx.R();
            o << "  " << tr  << " = call i32 @rq_truthy(%RqValue " << v << ")\n";
            o << "  " << cmp << " = icmp ne i32 " << tr << ", 0\n";
            o << "  br i1 " << cmp << ", label %_ip" << (ip+1) << ", label %_ip" << ins.a << "\n";
            break;
        }

        case OpCode::JumpIfNotNull: {
            auto v=peek(ctx,o); auto tp=ctx.R(); auto cmp=ctx.R();
            o << "  " << tp  << " = extractvalue %RqValue " << v << ", 0\n";
            o << "  " << cmp << " = icmp ne i32 " << tp << ", 0\n";
            o << "  br i1 " << cmp << ", label %_ip" << ins.b << ", label %_ip" << (ip+1) << "\n";
            break;
        }

        case OpCode::Return: {
            if (isEntry) { o << "  br label %_ip" << fn.code.size() << "\n"; }
            else {
                auto sp1=ctx.R(); auto sp0=ctx.R(); auto hasVal=ctx.R();
                auto rblk="__ret"+std::to_string(ctx.reg++);
                auto nblk="__rnull"+std::to_string(ctx.reg++);
                auto mblk="__rmrg"+std::to_string(ctx.reg++);
                o << "  " << sp1 << " = load i32, i32* " << ctx.spVar << "\n";
                o << "  " << sp0 << " = sub i32 " << sp1 << ", 1\n";
                o << "  " << hasVal << " = icmp sge i32 " << sp0 << ", 0\n";
                o << "  br i1 " << hasVal << ", label %" << rblk << ", label %" << nblk << "\n";
                o << rblk << ":\n";
                auto rv=pop(ctx,o);
                o << "  br label %" << mblk << "\n";
                o << nblk << ":\n";
                auto nv=rt0(ctx,o,"rq_null");
                o << "  br label %" << mblk << "\n";
                o << mblk << ":\n";
                auto phi=ctx.R();
                o << "  " << phi << " = phi %RqValue [ " << rv << ", %" << rblk
                  << " ], [ " << nv << ", %" << nblk << " ]\n";
                o << "  ret %RqValue " << phi << "\n";
            }
            break;
        }

        case OpCode::Throw: {
            auto v=pop(ctx,o); auto msg=ctx.R();
            o << "  " << msg << " = call i8* @rq_to_cstr(%RqValue " << v << ")\n";
            o << "  call void @rq_throw_msg(i8* " << msg << ")\n";
            o << "  unreachable\n";
            break;
        }

        case OpCode::Concat: {
            int n = ins.a;
            auto arr=ctx.R();
            o << "  " << arr << " = alloca %RqValue, i32 " << std::max(n,1) << "\n";
            std::vector<std::string> parts(n);
            for (int i=n-1;i>=0;--i) parts[i]=pop(ctx,o);
            for (int i=0;i<n;++i) {
                auto ptr=ctx.R();
                o << "  " << ptr << " = getelementptr %RqValue, %RqValue* " << arr << ", i32 " << i << "\n";
                o << "  store %RqValue " << parts[i] << ", %RqValue* " << ptr << "\n";
            }
            auto res=ctx.R();
            o << "  " << res << " = call %RqValue @rq_concat_n(%RqValue* " << arr << ", i32 " << n << ")\n";
            push(ctx,res,o); fallthrough(); break;
        }

        case OpCode::Call: {
            std::string callee = cname(ins.a);
            int argc = ins.b;
            std::vector<std::string> args(argc);
            for (int i=argc-1;i>=0;--i) args[i]=pop(ctx,o);
            auto res = emitCallExpr(ctx, o, callee, args, program);
            push(ctx,res,o); fallthrough(); break;
        }

        case OpCode::TryBegin: fallthrough(); break;
        case OpCode::TryEnd:   o << "  br label %_ip" << ins.a << "\n"; break;

        default:
            push(ctx,rt0(ctx,o,"rq_null"),o); fallthrough(); break;
        }
    }

    void emitModule(const BytecodeProgram& program) {
        out_ << "; RayQuiro LLVM IR\n";
        out_ << "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n";
        out_ << "target triple = \"" << targetTriple_ << "\"\n\n";

        emitRuntimeDecls();

        collectGlobals(program.entry);
        for (const auto& [name, fn] : program.functions) collectGlobals(fn);
        if (!globals_declared_.empty()) out_ << "\n";

        std::ostringstream funBuf;

        auto emitToFunBuf = [&](const BytecodeFunction& fn, bool isEntry) {
            std::ostringstream tmp;
            std::swap(out_, tmp);
            emitFunction(program, fn, isEntry);
            funBuf << out_.str();
            std::swap(out_, tmp);
        };

        for (const auto& [name, fn] : program.functions) emitToFunBuf(fn, false);
        emitToFunBuf(program.entry, true);

        out_ << strBuf_.str();
        if (strConst_ > 0) out_ << "\n";
        out_ << funBuf.str();

        out_ << "define i32 @main(i32 %argc, i8** %argv) {\n";
        out_ << "entry:\n";
        out_ << "  call void @_rq_main()\n";
        out_ << "  ret i32 0\n";
        out_ << "}\n";
    }
};
