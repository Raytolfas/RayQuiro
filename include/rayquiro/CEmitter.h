#pragma once



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

        e.program_ = &program;

        e.emitProgram(program);

        return e.out_.str();

    }



private:

    std::ostringstream out_;

    std::unordered_set<std::string> globals_declared_;

    const BytecodeProgram* program_ = nullptr;



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

            else if (c == '"')   r += "\\\"";

            else if (c == '\n')  r += "\\n";

            else if (c == '\r')  r += "\\r";

            else if (c == '\t')  r += "\\t";

            else if (c < 32)   { char buf[8]; snprintf(buf,sizeof(buf),"\\x%02x",c); r+=buf; }

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

            return "rq_str_lit(" + escStr(*s) + ")";

        if (auto* b = std::get_if<bool>(&v.data))

            return *b ? "rq_bool(1)" : "rq_bool(0)";

        return "rq_null()";

    }



    void collectGlobals(const BytecodeFunction& fn) {

        for (const auto& ins : fn.code) {

            auto op = ins.op;

            if (op == OpCode::TryBegin) {

                const auto& cval = fn.constants.at((size_t)ins.b);

                if (std::holds_alternative<std::string>(cval.data)) {

                    const std::string& name = std::get<std::string>(cval.data);

                    if (!name.empty() && globals_declared_.insert(name).second)

                        out_ << "static RqValue _g_" << safeName(name) << ";\n";

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

        out_ << "    RqCatchFrame _cf_pool[16]; int _cf_idx=0; (void)_cf_idx;\n";

        out_ << "    RqCatchFrame* _fn_catch_entry = _rq_catch_top;\n";



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

        else out_ << "    _rq_catch_top = _fn_catch_entry;\n    return rq_null();\n}\n";

    }



    void emitInstr(const BytecodeFunction& fn, size_t ip) {

        (void)ip;

        const Instruction& ins = fn.code[ip];

        out_ << "    ";



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

            out_ << "_rq_catch_top = _fn_catch_entry;\n";

            if (fn.name.empty()) out_ << "return;\n";

            else out_ << "{RqValue _ret=(_sp>0)?_stack[--_sp]:rq_null();return _ret;}\n";

            break;



        case OpCode::Throw:

            out_ << "{\n"

                 << "  RqValue _thrown = (_sp > 0) ? _stack[--_sp] : rq_null();\n"

                 << "  if (_thrown.type != RQ_STR) { _thrown = rq_to_str(_thrown); }\n"

                 << "  _rq_last_error = _thrown;\n"

                 << "  if (_rq_catch_top) {\n"

                 << "    longjmp(_rq_catch_top->jbuf, 1);\n"

                 << "  } else {\n"

                 << "    char* _msg = rq_to_cstr(_thrown);\n"

                 << "    fprintf(stderr, \"error: %s\\n\", _msg);\n"

                 << "    free(_msg);\n"

                 << "    exit(1);\n"

                 << "  }\n"

                 << "}\n";

            break;



        case OpCode::Concat: {

            int n = ins.a;

            out_ << "{RqValue _pts[" << n << "];for(int _ci=" << (n-1) << ";_ci>=0;_ci--)_pts[_ci]=_stack[--_sp];\n"

                 << "  size_t _tl=0;char* _pp[" << n << "];for(int _ci=0;_ci<" << n << ";_ci++){_pp[_ci]=rq_to_cstr(_pts[_ci]);_tl+=strlen(_pp[_ci]);}\n"

                 << "  char* _cr=(char*)malloc(_tl+1);_cr[0]=0;for(int _ci=0;_ci<" << n << ";_ci++){strcat(_cr,_pp[_ci]);free(_pp[_ci]);}\n"

                 << "  RqValue _rs;_rs.type=RQ_STR;_rs.d.str=_cr;_stack[_sp++]=_rs;}\n";

            break;

        }



        case OpCode::TryBegin: {

            std::string errVar = cname(ins.b);

            out_ << "if (_cf_idx < 16) {\n"

                 << "  RqCatchFrame* _cf = &_cf_pool[_cf_idx++];\n"

                 << "  strncpy(_cf->err_var, \"" << safeName(errVar) << "\", 63);\n"

                 << "  _cf->err_var[63] = 0;\n"

                 << "  _cf->sp = _sp;\n"

                 << "  _cf->prev = _rq_catch_top;\n"

                 << "  _rq_catch_top = _cf;\n"

                 << "  if (setjmp(_cf->jbuf) != 0) {\n"

                 << "    _rq_catch_top = _cf->prev;\n"

                 << "    _sp = _cf->sp;\n";

            if (!errVar.empty()) {

                out_ << "    _g_" << safeName(errVar) << " = _rq_last_error;\n";

            }

            out_ << "    goto _L" << ins.a << ";\n"

                 << "  }\n"

                 << "}\n";

            break;

        }

        case OpCode::TryEnd:

            out_ << "if (_rq_catch_top) { _rq_catch_top = _rq_catch_top->prev; if (_cf_idx > 0) _cf_idx--; }\n";

            out_ << "goto _L" << ins.a << ";\n";

            break;



        case OpCode::Call: {

            std::string callee = cname(ins.a);

            int argc = ins.b;

            out_ << "{RqValue _ca[" << std::max(argc,1) << "];";

            out_ << "for(int _ai=" << (argc-1) << ";_ai>=0;_ai--)_ca[_ai]=_stack[--_sp];";

            out_ << "_stack[_sp++]=" << emitCallExpr(callee, argc) << ";}\n";

            break;

        }



        case OpCode::BuildArray: {

            int cnt = ins.a;

            out_ << "{RqValue _arr=rq_arr_new(" << std::max(cnt, 8) << ");";

            if (cnt > 0) {

                out_ << "for(int _ai=0;_ai<" << cnt << ";_ai++){rq_push(_arr,_stack[_sp-" << cnt << "+_ai]);}";

                out_ << "_sp-=" << cnt << ";";

            }

            out_ << "_stack[_sp++]=_arr;}\n";

            break;

        }

        case OpCode::BuildObject: {

            int cnt = ins.a;

            out_ << "{\n"

                 << "  RqValue _obj = rq_obj_new(" << std::max(cnt + 8, 8) << ");\n";

            if (cnt > 0) {

                out_ << "  for (int _bi = 0; _bi < " << cnt << "; _bi++) {\n"

                     << "    int _base = _sp - 2 * (" << cnt << " - _bi);\n"

                     << "    RqValue _k = _stack[_base];\n"

                     << "    RqValue _v = _stack[_base + 1];\n"

                     << "    char* _ks = rq_to_cstr(_k);\n"

                     << "    if (_bi == 0 && strcmp(_ks, \"__type__\") == 0 && _v.type == RQ_STR) {\n"

                     << "      rq_obj_copy_defaults(_obj, rq_get_struct_proto(_v.d.str));\n"

                     << "    }\n"

                     << "    rq_obj_set(_obj, _ks, _v);\n"

                     << "    free(_ks);\n"

                     << "  }\n"

                     << "  RqValue _sname = rq_obj_get(_obj, \"__struct__\");\n"

                     << "  if (_sname.type == RQ_STR && _sname.d.str) {\n"

                     << "    rq_register_struct(_sname.d.str, _obj);\n"

                     << "  }\n"

                     << "  _sp -= " << (2 * cnt) << ";\n";

            }

            out_ << "  _stack[_sp++] = _obj;\n}\n";

            break;

        }

        case OpCode::GetIndex:

            out_ << "{RqValue _idx=_stack[--_sp],_tgt=_stack[--_sp];_stack[_sp++]=rq_get_index(_tgt,_idx);}\n"; break;

        case OpCode::SetIndex:

            out_ << "{RqValue _val=_stack[--_sp],_idx=_stack[--_sp],_tgt=_stack[--_sp];rq_set_index(_tgt,_idx,_val);_stack[_sp++]=_val;}\n"; break;

        case OpCode::SetLocalIndex:

            out_ << "{RqValue _val=_stack[--_sp],_idx=_stack[--_sp];rq_set_index(_locals[" << ins.a << "],_idx,_val);_stack[_sp++]=_val;}\n"; break;

        case OpCode::SetGlobalIndex:

            out_ << "{RqValue _val=_stack[--_sp],_idx=_stack[--_sp];rq_set_index(_g_" << safeName(cname(ins.a)) << ",_idx,_val);_stack[_sp++]=_val;}\n"; break;

        case OpCode::AppendGlobal:

            out_ << "{RqValue _v=_stack[--_sp];rq_push(_g_" << safeName(cname(ins.a)) << ",_v);_stack[_sp++]=_v;}\n"; break;

        case OpCode::AppendLocal:

            out_ << "{RqValue _v=_stack[--_sp];rq_push(_locals[" << ins.a << "],_v);_stack[_sp++]=_v;}\n"; break;

        case OpCode::AppendObjGlobal:

            out_ << "{RqValue _v=_stack[--_sp],_k=_stack[--_sp];char* _ks=rq_to_cstr(_k);RqValue _sub=rq_obj_get(_g_" << safeName(cname(ins.a)) << ",_ks);if(_sub.type!=RQ_ARR){_sub=rq_arr_new(4);rq_obj_set(_g_" << safeName(cname(ins.a)) << ",_ks,_sub);}rq_push(_sub,_v);free(_ks);_stack[_sp++]=_v;}\n"; break;

        case OpCode::AppendObjLocal:

            out_ << "{RqValue _v=_stack[--_sp],_k=_stack[--_sp];char* _ks=rq_to_cstr(_k);RqValue _sub=rq_obj_get(_locals[" << ins.a << "],_ks);if(_sub.type!=RQ_ARR){_sub=rq_arr_new(4);rq_obj_set(_locals[" << ins.a << "],_ks,_sub);}rq_push(_sub,_v);free(_ks);_stack[_sp++]=_v;}\n"; break;

        default:

            out_ << "if(_sp>0)_sp--;\n"; break;

        }

    }



    std::string emitCallExpr(const std::string& callee, int argc) {

        if (callee=="print")       return argc > 0 ? "rq_print(_ca[0])" : "rq_print(rq_null())";

        if (callee=="str")         return argc > 0 ? "rq_str_builtin(_ca[0])" : "rq_str_builtin(rq_null())";

        if (callee=="num")         return argc > 0 ? "rq_num_builtin(_ca[0])" : "rq_num_builtin(rq_null())";

        if (callee=="bool")        return argc > 0 ? "rq_bool_builtin(_ca[0])" : "rq_bool_builtin(rq_null())";

        if (callee=="type")        return argc > 0 ? "rq_type_builtin(_ca[0])" : "rq_type_builtin(rq_null())";

        if (callee=="assert")      return argc > 0 ? "rq_assert(_ca[0])" : "rq_assert(rq_bool(1))";

        if (callee=="assert_true") return argc > 0 ? "rq_assert_true(_ca[0])" : "rq_assert_true(rq_bool(1))";

        if (callee=="assert_false")return argc > 0 ? "rq_assert_false(_ca[0])" : "rq_assert_false(rq_bool(0))";

        if (callee=="assert_null") return argc > 0 ? "rq_assert_null(_ca[0])" : "rq_assert_null(rq_null())";

        if (callee=="assert_eq")   return argc >= 2 ? "rq_assert_eq(_ca[0],_ca[1])" : "rq_assert_eq(rq_null(),rq_null())";

        if (callee=="assert_ne")   return argc >= 2 ? "rq_assert_ne(_ca[0],_ca[1])" : "rq_assert_ne(rq_null(),rq_null())";

        if (callee=="clock.ms" || callee=="time.now_ms" || callee=="clock_ms") return "rq_clock_ms()";

        if (callee=="to_string")   return argc > 0 ? "rq_str_builtin(_ca[0])" : "rq_str_builtin(rq_null())";

        if (callee=="len" || callee=="length") return argc > 0 ? "rq_len(_ca[0])" : "rq_num(0.0)";

        if (callee=="push")        return argc >= 2 ? "rq_push(_ca[0],_ca[1])" : "rq_null()";

        if (callee=="pop")         return argc > 0 ? "rq_pop(_ca[0])" : "rq_null()";

        if (callee=="contains")    return argc >= 2 ? "rq_contains(_ca[0],_ca[1])" : "rq_bool(0)";

        if (callee=="startswith")  return argc >= 2 ? "rq_startswith(_ca[0],_ca[1])" : "rq_bool(0)";

        if (callee=="endswith")    return argc >= 2 ? "rq_endswith(_ca[0],_ca[1])" : "rq_bool(0)";

        if (callee=="indexof")     return argc >= 2 ? "rq_indexof(_ca[0],_ca[1])" : "rq_num(-1.0)";

        if (callee=="upper")       return argc > 0 ? "rq_upper(_ca[0])" : "rq_str_lit(\"\")";

        if (callee=="lower")       return argc > 0 ? "rq_lower(_ca[0])" : "rq_str_lit(\"\")";

        if (callee=="trim")        return argc > 0 ? "rq_trim(_ca[0])" : "rq_str_lit(\"\")";

        if (callee=="replace")     return argc >= 3 ? "rq_replace(_ca[0],_ca[1],_ca[2])" : (argc >= 2 ? "rq_replace(_ca[0],_ca[1],rq_str_lit(\"\"))" : "rq_null()");

        if (callee=="slice")       return argc >= 3 ? "rq_slice(_ca[0],_ca[1],_ca[2])" : (argc == 2 ? "rq_slice(_ca[0],_ca[1],rq_null())" : "rq_slice(_ca[0],rq_null(),rq_null())");

        if (callee=="range")       return argc >= 2 ? "rq_range(_ca[0],_ca[1])" : (argc == 1 ? "rq_range(rq_num(0.0),_ca[0])" : "rq_arr_new(0)");

        if (callee=="range_inclusive") return argc >= 2 ? "rq_range_inclusive(_ca[0],_ca[1])" : "rq_arr_new(0)";

        if (callee=="join")        return argc >= 2 ? "rq_join(_ca[0],_ca[1])" : (argc == 1 ? "rq_join(_ca[0],rq_str_lit(\"\"))" : "rq_str_lit(\"\")");

        if (callee=="repeat")      return argc >= 2 ? "rq_repeat(_ca[0],_ca[1])" : "rq_str_lit(\"\")";

        if (callee=="char" || callee=="char_from" || callee=="char_code") return argc > 0 ? "rq_char(_ca[0])" : "rq_str_lit(\"\")";

        if (callee=="math.sqrt" || callee=="sqrt") return argc > 0 ? "rq_sqrt(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.abs"  || callee=="abs")  return argc > 0 ? "rq_abs(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.floor"|| callee=="floor")return argc > 0 ? "rq_floor(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.ceil" || callee=="ceil") return argc > 0 ? "rq_ceil(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.round"|| callee=="round")return argc > 0 ? "rq_round(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.pow"  || callee=="pow")  return argc >= 2 ? "rq_pow(_ca[0],_ca[1])" : "rq_num(0.0)";

        if (callee=="math.sin"  || callee=="sin")  return argc > 0 ? "rq_sin(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.cos"  || callee=="cos")  return argc > 0 ? "rq_cos(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.tan"  || callee=="tan")  return argc > 0 ? "rq_tan(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.log"  || callee=="log")  return argc > 0 ? "rq_log(_ca[0])" : "rq_num(0.0)";

        if (callee=="math.min"  || callee=="min")  return argc >= 2 ? "rq_min(_ca[0],_ca[1])" : (argc == 1 ? "_ca[0]" : "rq_num(0.0)");

        if (callee=="math.max"  || callee=="max")  return argc >= 2 ? "rq_max(_ca[0],_ca[1])" : (argc == 1 ? "_ca[0]" : "rq_num(0.0)");
        if (callee=="math.clamp"|| callee=="clamp")return argc >= 3 ? "rq_clamp(_ca[0],_ca[1],_ca[2])" : (argc == 1 ? "_ca[0]" : "rq_num(0.0)");
        if (callee=="vec2" || callee=="engine.vec2") return argc >= 2 ? "rq_vec2(_ca[0],_ca[1])" : "rq_vec2(rq_num(0.0),rq_num(0.0))";
        if (callee=="vec3" || callee=="engine.vec3") return argc >= 3 ? "rq_vec3(_ca[0],_ca[1],_ca[2])" : "rq_vec3(rq_num(0.0),rq_num(0.0),rq_num(0.0))";
        if (callee=="vec_add") return argc >= 2 ? "rq_vec_add(_ca[0],_ca[1])" : "rq_null()";
        if (callee=="vec_sub") return argc >= 2 ? "rq_vec_sub(_ca[0],_ca[1])" : "rq_null()";
        if (callee=="vec_mul") return argc >= 2 ? "rq_vec_mul(_ca[0],_ca[1])" : "rq_null()";
        if (callee=="vec_dot") return argc >= 2 ? "rq_vec_dot(_ca[0],_ca[1])" : "rq_num(0.0)";
        if (callee=="vec_len") return argc > 0 ? "rq_vec_len(_ca[0])" : "rq_num(0.0)";
        if (callee=="vec_normalize") return argc > 0 ? "rq_vec_normalize(_ca[0])" : "rq_null()";
        if (callee=="vec_cross") return argc >= 2 ? "rq_vec_cross(_ca[0],_ca[1])" : "rq_null()";
        if (callee=="json.stringify" || callee=="json_stringify") return argc > 0 ? "rq_json_stringify(_ca[0])" : "rq_str_lit(\"null\")";
        if (callee=="json.parse" || callee=="json_parse") return argc > 0 ? "rq_json_parse(_ca[0])" : "rq_null()";

        size_t expectedParams = (size_t)argc;

        bool isVar = false;

        if (program_) {

            auto fit = program_->functions.find(callee);

            if (fit != program_->functions.end()) {

                expectedParams = fit->second.params.size();

                isVar = fit->second.isVariadic;

            }

        }

        if (isVar && expectedParams > 0) {

            size_t regCount = expectedParams - 1;

            std::string r = "({ RqValue _vargs = rq_arr_new(); ";

            for (size_t i = regCount; i < (size_t)argc; i++) {

                r += "rq_arr_push(&_vargs, _ca[" + std::to_string(i) + "]); ";

            }

            r += "_rq_fn_" + safeName(callee) + "(";

            for (size_t i = 0; i < regCount; i++) {

                if (i) r += ",";

                if (i < (size_t)argc) r += "_ca[" + std::to_string(i) + "]";

                else r += "rq_null()";

            }

            if (regCount > 0) r += ",";

            r += "_vargs); })";

            return r;

        }

        std::string r = "_rq_fn_" + safeName(callee) + "(";

        for (size_t i = 0; i < expectedParams; i++) {

            if (i) r += ",";

            if (i < (size_t)argc) r += "_ca[" + std::to_string(i) + "]";

            else r += "rq_null()";

        }

        r += ")";

        return r;

    }



    void emitRuntime() {

        out_ << "#include <stdio.h>\n"

             << "#include <stdlib.h>\n"

             << "#include <string.h>\n"

             << "#include <math.h>\n"

             << "#include <stdint.h>\n"

             << "#include <ctype.h>\n"

             << "#include <setjmp.h>\n\n"

             << "#if defined(_WIN32) && !defined(__TINYC__)\n"
             << "#ifndef WIN32_LEAN_AND_MEAN\n"
             << "#define WIN32_LEAN_AND_MEAN\n"
             << "#endif\n"
             << "#include <windows.h>\n"
             << "#else\n"
             << "#include <time.h>\n"
             << "#if defined(_WIN32) && defined(__TINYC__)\n"
             << "unsigned long __stdcall GetCurrentDirectoryA(unsigned long, char*);\n"
             << "int __cdecl _putenv_s(const char*, const char*);\n"
             << "int __stdcall SetEnvironmentVariableA(const char*, const char*);\n"
             << "#endif\n"
             << "#endif\n\n"

             << "typedef enum { RQ_NULL = 0, RQ_NUM = 1, RQ_STR = 2, RQ_BOOL = 3, RQ_ARR = 4, RQ_OBJ = 5 } RqType;\n"

             << "struct RqArray;\n"

             << "struct RqObject;\n"

             << "typedef struct RqValue { RqType type; union { double num; char* str; int b; struct RqArray* arr; struct RqObject* obj; } d; } RqValue;\n"

             << "typedef struct RqArray { RqValue* items; int size; int capacity; } RqArray;\n"

             << "typedef struct RqObjectEntry { char* key; RqValue val; } RqObjectEntry;\n"

             << "typedef struct RqObject { RqObjectEntry* entries; int size; int capacity; } RqObject;\n"

             << "typedef struct RqCatchFrame { jmp_buf jbuf; char err_var[64]; int sp; struct RqCatchFrame* prev; } RqCatchFrame;\n"

             << "static RqCatchFrame* _rq_catch_top = NULL;\n"

             << "static RqValue _rq_last_error;\n"

             << "static inline RqValue rq_null(void)       { RqValue v; v.type=RQ_NULL; v.d.num=0; return v; }\n"

             << "static inline RqValue rq_num(double n)    { RqValue v; v.type=RQ_NUM; v.d.num=n; return v; }\n"

             << "static inline RqValue rq_bool(int b)      { RqValue v; v.type=RQ_BOOL; v.d.b=(b!=0); return v; }\n"

             << "static inline RqValue rq_str_lit(const char* src) {\n"

             << "    RqValue v; v.type=RQ_STR;\n"

             << "    v.d.str=(char*)(src?src:\"\");\n"

             << "    return v;\n"

             << "}\n"

             << "static inline RqValue rq_str_copy(const char* src) {\n"

             << "    RqValue v; v.type=RQ_STR;\n"

             << "    if (!src) { v.d.str=(char*)malloc(1); v.d.str[0]=0; return v; }\n"

             << "    size_t l=strlen(src); v.d.str=(char*)malloc(l+1); memcpy(v.d.str,src,l+1); return v;\n"

             << "}\n"

             << "static inline RqValue rq_arr_new(int cap) {\n"

             << "    if (cap < 8) cap = 8;\n"

             << "    RqArray* a = (RqArray*)malloc(sizeof(RqArray));\n"

             << "    a->items = (RqValue*)malloc(sizeof(RqValue) * (size_t)cap);\n"

             << "    a->size = 0; a->capacity = cap;\n"

             << "    RqValue v; v.type = RQ_ARR; v.d.arr = a;\n"

             << "    return v;\n"

             << "}\n"

             << "static inline RqValue rq_push(RqValue arrVal, RqValue item) {\n"

             << "    if (arrVal.type == RQ_ARR && arrVal.d.arr) {\n"

             << "        RqArray* a = arrVal.d.arr;\n"

             << "        if (a->size >= a->capacity) {\n"

             << "            a->capacity = a->capacity < 8 ? 8 : a->capacity * 2;\n"

             << "            a->items = (RqValue*)realloc(a->items, sizeof(RqValue) * (size_t)a->capacity);\n"

             << "        }\n"

             << "        a->items[a->size++] = item;\n"

             << "    }\n"

             << "    return arrVal;\n"

             << "}\n"

             << "static inline RqValue rq_pop(RqValue arrVal) {\n"

             << "    if (arrVal.type == RQ_ARR && arrVal.d.arr && arrVal.d.arr->size > 0) {\n"

             << "        return arrVal.d.arr->items[--arrVal.d.arr->size];\n"

             << "    }\n"

             << "    return rq_null();\n"

             << "}\n"

             << "static inline RqValue rq_len(RqValue v) {\n"

             << "    if (v.type == RQ_ARR && v.d.arr) return rq_num((double)v.d.arr->size);\n"

             << "    if (v.type == RQ_STR && v.d.str) return rq_num((double)strlen(v.d.str));\n"

             << "    if (v.type == RQ_OBJ && v.d.obj) return rq_num((double)v.d.obj->size);\n"

             << "    return rq_num(0.0);\n"

             << "}\n"

             << "static inline RqValue rq_obj_new(int cap) {\n"

             << "    if (cap < 4) cap = 4;\n"

             << "    RqObject* o = (RqObject*)malloc(sizeof(RqObject));\n"

             << "    o->entries = (RqObjectEntry*)malloc(sizeof(RqObjectEntry) * (size_t)cap);\n"

             << "    o->size = 0; o->capacity = cap;\n"

             << "    RqValue v; v.type = RQ_OBJ; v.d.obj = o; return v;\n"

             << "}\n"

             << "static inline void rq_obj_set(RqValue objVal, const char* key, RqValue val) {\n"

             << "    if (objVal.type != RQ_OBJ || !objVal.d.obj || !key) return;\n"

             << "    RqObject* o = objVal.d.obj;\n"

             << "    for (int i = 0; i < o->size; i++) {\n"

             << "        if (strcmp(o->entries[i].key, key) == 0) { o->entries[i].val = val; return; }\n"

             << "    }\n"

             << "    if (o->size >= o->capacity) {\n"

             << "        o->capacity = o->capacity < 4 ? 4 : o->capacity * 2;\n"

             << "        o->entries = (RqObjectEntry*)realloc(o->entries, sizeof(RqObjectEntry) * (size_t)o->capacity);\n"

             << "    }\n"

             << "    size_t kl = strlen(key); char* kc = (char*)malloc(kl + 1); memcpy(kc, key, kl + 1);\n"

             << "    o->entries[o->size].key = kc; o->entries[o->size].val = val; o->size++;\n"

             << "}\n"

             << "static inline RqValue rq_obj_get(RqValue objVal, const char* key) {\n"

             << "    if (objVal.type != RQ_OBJ || !objVal.d.obj || !key) return rq_null();\n"

             << "    RqObject* o = objVal.d.obj;\n"

             << "    for (int i = 0; i < o->size; i++) {\n"

             << "        if (strcmp(o->entries[i].key, key) == 0) return o->entries[i].val;\n"

             << "    }\n"

             << "    return rq_null();\n"

             << "}\n"

             << "typedef struct RqStructDef { char name[64]; RqValue proto; } RqStructDef;\n"

             << "static RqStructDef _rq_structs[64];\n"

             << "static int _rq_struct_count = 0;\n"

             << "static inline void rq_register_struct(const char* name, RqValue proto) {\n"

             << "    if (!name || _rq_struct_count >= 64) return;\n"

             << "    for (int i = 0; i < _rq_struct_count; i++) {\n"

             << "        if (strcmp(_rq_structs[i].name, name) == 0) { _rq_structs[i].proto = proto; return; }\n"

             << "    }\n"

             << "    strncpy(_rq_structs[_rq_struct_count].name, name, 63);\n"

             << "    _rq_structs[_rq_struct_count].name[63] = 0;\n"

             << "    _rq_structs[_rq_struct_count].proto = proto;\n"

             << "    _rq_struct_count++;\n"

             << "}\n"

             << "static inline RqValue rq_get_struct_proto(const char* name) {\n"

             << "    if (!name) return rq_null();\n"

             << "    for (int i = 0; i < _rq_struct_count; i++) {\n"

             << "        if (strcmp(_rq_structs[i].name, name) == 0) return _rq_structs[i].proto;\n"

             << "    }\n"

             << "    return rq_null();\n"

             << "}\n"

             << "static inline void rq_obj_copy_defaults(RqValue targetObj, RqValue protoObj) {\n"

             << "    if (targetObj.type != RQ_OBJ || !targetObj.d.obj || protoObj.type != RQ_OBJ || !protoObj.d.obj) return;\n"

             << "    RqObject* proto = protoObj.d.obj;\n"

             << "    for (int i = 0; i < proto->size; i++) {\n"

             << "        if (strcmp(proto->entries[i].key, \"__struct__\") != 0) {\n"

             << "            rq_obj_set(targetObj, proto->entries[i].key, proto->entries[i].val);\n"

             << "        }\n"

             << "    }\n"

             << "}\n\n"

             << "static inline char* rq_to_cstr(RqValue v);\n"

             << "static inline RqValue rq_get_index(RqValue target, RqValue indexVal) {\n"

             << "    if (target.type == RQ_ARR && target.d.arr) {\n"

             << "        int idx = (int)indexVal.d.num;\n"

             << "        if (idx >= 0 && idx < target.d.arr->size) return target.d.arr->items[idx];\n"

             << "    }\n"

             << "    if (target.type == RQ_OBJ && target.d.obj) {\n"

             << "        char* k = rq_to_cstr(indexVal);\n"

             << "        RqValue r = rq_obj_get(target, k);\n"

             << "        free(k); return r;\n"

             << "    }\n"

             << "    return rq_null();\n"

             << "}\n"

             << "static inline void rq_set_index(RqValue target, RqValue indexVal, RqValue val) {\n"

             << "    if (target.type == RQ_ARR && target.d.arr) {\n"

             << "        int idx = (int)indexVal.d.num;\n"

             << "        if (idx >= 0 && idx < target.d.arr->size) target.d.arr->items[idx] = val;\n"

             << "        else if (idx == target.d.arr->size) rq_push(target, val);\n"

             << "    }\n"

             << "    if (target.type == RQ_OBJ && target.d.obj) {\n"

             << "        char* k = rq_to_cstr(indexVal);\n"

             << "        rq_obj_set(target, k, val);\n"

             << "        free(k);\n"

             << "    }\n"

             << "}\n"

             << "static inline int rq_truthy(RqValue v) {\n"

             << "    switch(v.type){case RQ_NULL:return 0;case RQ_BOOL:return v.d.b;\n"

             << "    case RQ_NUM:return v.d.num!=0.0;case RQ_STR:return v.d.str&&v.d.str[0]!=0;case RQ_ARR:return v.d.arr&&v.d.arr->size>0;case RQ_OBJ:return v.d.obj&&v.d.obj->size>0;default:return 0;}\n"

             << "}\n"

             << "static inline char* rq_to_cstr(RqValue v) {\n"

             << "    char buf[64];\n"

             << "    switch(v.type){\n"

             << "    case RQ_NULL: return strdup(\"null\");\n"

             << "    case RQ_BOOL: return strdup(v.d.b?\"true\":\"false\");\n"

             << "    case RQ_STR:  return strdup(v.d.str?v.d.str:\"\");\n"

             << "    case RQ_ARR:  return strdup(\"[array]\");\n"

             << "    case RQ_OBJ:  return strdup(\"[object]\");\n"

             << "    case RQ_NUM: {\n"

             << "        double n=v.d.num;\n"

             << "        if(n==(long long)n&&n>=-1e15&&n<=1e15) snprintf(buf,sizeof(buf),\"%lld\",(long long)n);\n"

             << "        else snprintf(buf,sizeof(buf),\"%g\",n);\n"

             << "        return strdup(buf);}\n"

             << "    default: return strdup(\"\");}\n"

             << "}\n"

             << "static inline RqValue rq_to_str(RqValue v){char*s=rq_to_cstr(v);RqValue r=rq_str_copy(s);free(s);return r;}\n\n"

             << "#if defined(_WIN32) && !defined(__TINYC__)\n"
             << "static inline RqValue rq_clock_ms(void) {\n"
             << "    static LARGE_INTEGER freq;\n"
             << "    static int init = 0;\n"
             << "    if (!init) { QueryPerformanceFrequency(&freq); init = 1; }\n"
             << "    LARGE_INTEGER c;\n"
             << "    QueryPerformanceCounter(&c);\n"
             << "    return rq_num((double)c.QuadPart * 1000.0 / (double)freq.QuadPart);\n"
             << "}\n"
             << "#elif defined(_WIN32)\n"
             << "static inline RqValue rq_clock_ms(void) {\n"
             << "    return rq_num((double)clock() * 1000.0 / CLOCKS_PER_SEC);\n"
             << "}\n"
             << "#else\n"
             << "static inline RqValue rq_clock_ms(void) {\n"
             << "    struct timespec ts;\n"
             << "    clock_gettime(CLOCK_MONOTONIC, &ts);\n"
             << "    return rq_num((double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0);\n"
             << "}\n"
             << "#endif\n\n"

             << "static inline RqValue rq_sin(RqValue v){return rq_num(sin(v.d.num));}\n"

             << "static inline RqValue rq_cos(RqValue v){return rq_num(cos(v.d.num));}\n"

             << "static inline RqValue rq_tan(RqValue v){return rq_num(tan(v.d.num));}\n"

             << "static inline RqValue rq_log(RqValue v){return rq_num(log(v.d.num));}\n"

             << "static inline RqValue rq_min(RqValue a,RqValue b){return rq_num(a.d.num<b.d.num?a.d.num:b.d.num);}\n"

             << "static inline RqValue rq_max(RqValue a,RqValue b){return rq_num(a.d.num>b.d.num?a.d.num:b.d.num);}\n"

             << "static inline RqValue rq_add(RqValue a,RqValue b){\n"

             << "    if(a.type==RQ_STR||b.type==RQ_STR){\n"

             << "        char*sa=(a.type==RQ_STR&&a.d.str)?a.d.str:rq_to_cstr(a);\n"

             << "        char*sb=(b.type==RQ_STR&&b.d.str)?b.d.str:rq_to_cstr(b);\n"

             << "        size_t la=strlen(sa),lb=strlen(sb);\n"

             << "        char*res=(char*)malloc(la+lb+1);memcpy(res,sa,la);memcpy(res+la,sb,lb+1);\n"

             << "        if(a.type!=RQ_STR) free(sa);\n"

             << "        if(b.type!=RQ_STR) free(sb);\n"

             << "        RqValue r;r.type=RQ_STR;r.d.str=res;return r;}\n"

             << "    return rq_num(a.d.num+b.d.num);}\n"

             << "static inline RqValue rq_sub(RqValue a,RqValue b){return rq_num(a.d.num-b.d.num);}\n"

             << "static inline RqValue rq_mul(RqValue a,RqValue b){return rq_num(a.d.num*b.d.num);}\n"

             << "static inline RqValue rq_div(RqValue a,RqValue b){return rq_num(b.d.num!=0.0?a.d.num/b.d.num:0.0);}\n"

             << "static inline RqValue rq_mod(RqValue a,RqValue b){\n"

             << "    long long ia=(long long)a.d.num,ib=(long long)b.d.num;\n"

             << "    return rq_num(ib!=0?(double)(ia%ib):0.0);}\n"

             << "static inline RqValue rq_neg(RqValue a){return rq_num(-a.d.num);}\n"

             << "static inline RqValue rq_eq(RqValue a,RqValue b){\n"

             << "    if(a.type!=b.type)return rq_bool(0);\n"

             << "    switch(a.type){case RQ_NULL:return rq_bool(1);case RQ_BOOL:return rq_bool(a.d.b==b.d.b);\n"

             << "    case RQ_NUM:return rq_bool(a.d.num==b.d.num);case RQ_STR:return rq_bool(strcmp(a.d.str,b.d.str)==0);\n"

             << "    case RQ_ARR:return rq_bool(a.d.arr==b.d.arr);case RQ_OBJ:return rq_bool(a.d.obj==b.d.obj);\n"

             << "    default:return rq_bool(0);}}\n"

             << "static inline RqValue rq_ne(RqValue a,RqValue b){return rq_bool(!rq_truthy(rq_eq(a,b)));}\n"

             << "static inline RqValue rq_lt(RqValue a,RqValue b){return rq_bool(a.d.num<b.d.num);}\n"

             << "static inline RqValue rq_le(RqValue a,RqValue b){return rq_bool(a.d.num<=b.d.num);}\n"

             << "static inline RqValue rq_gt(RqValue a,RqValue b){return rq_bool(a.d.num>b.d.num);}\n"

             << "static inline RqValue rq_ge(RqValue a,RqValue b){return rq_bool(a.d.num>=b.d.num);}\n"

             << "static inline RqValue rq_not(RqValue a){return rq_bool(!rq_truthy(a));}\n"

             << "static inline RqValue rq_coalesce(RqValue a,RqValue b){return(a.type==RQ_NULL)?b:a;}\n\n"

             << "static inline RqValue rq_contains(RqValue target, RqValue item) {\n"

             << "    if (target.type == RQ_STR && item.type == RQ_STR && target.d.str && item.d.str) return rq_bool(strstr(target.d.str, item.d.str) != NULL);\n"

             << "    if (target.type == RQ_ARR && target.d.arr) {\n"

             << "        for (int i = 0; i < target.d.arr->size; i++) if (rq_truthy(rq_eq(target.d.arr->items[i], item))) return rq_bool(1);\n"

             << "    }\n"

             << "    return rq_bool(0);\n"

             << "}\n"

             << "static inline RqValue rq_startswith(RqValue target, RqValue prefix) {\n"

             << "    if (target.type == RQ_STR && prefix.type == RQ_STR && target.d.str && prefix.d.str) {\n"

             << "        size_t pl = strlen(prefix.d.str); return rq_bool(strncmp(target.d.str, prefix.d.str, pl) == 0);\n"

             << "    }\n"

             << "    return rq_bool(0);\n"

             << "}\n"

             << "static inline RqValue rq_endswith(RqValue target, RqValue suffix) {\n"

             << "    if (target.type == RQ_STR && suffix.type == RQ_STR && target.d.str && suffix.d.str) {\n"

             << "        size_t tl = strlen(target.d.str), sl = strlen(suffix.d.str);\n"

             << "        if (tl >= sl) return rq_bool(strcmp(target.d.str + (tl - sl), suffix.d.str) == 0);\n"

             << "    }\n"

             << "    return rq_bool(0);\n"

             << "}\n"

             << "static inline RqValue rq_indexof(RqValue target, RqValue item) {\n"

             << "    if (target.type == RQ_STR && item.type == RQ_STR && target.d.str && item.d.str) {\n"

             << "        char* pos = strstr(target.d.str, item.d.str);\n"

             << "        return rq_num(pos ? (double)(pos - target.d.str) : -1.0);\n"

             << "    }\n"

             << "    if (target.type == RQ_ARR && target.d.arr) {\n"

             << "        for (int i = 0; i < target.d.arr->size; i++) if (rq_truthy(rq_eq(target.d.arr->items[i], item))) return rq_num((double)i);\n"

             << "    }\n"

             << "    return rq_num(-1.0);\n"

             << "}\n"

             << "static inline RqValue rq_upper(RqValue v) {\n"

             << "    if (v.type != RQ_STR || !v.d.str) return rq_str_lit(\"\");\n"

             << "    size_t l = strlen(v.d.str); char* s = (char*)malloc(l + 1);\n"

             << "    for (size_t i = 0; i <= l; i++) s[i] = (char)toupper((unsigned char)v.d.str[i]);\n"

             << "    RqValue r; r.type = RQ_STR; r.d.str = s; return r;\n"

             << "}\n"

             << "static inline RqValue rq_lower(RqValue v) {\n"

             << "    if (v.type != RQ_STR || !v.d.str) return rq_str_lit(\"\");\n"

             << "    size_t l = strlen(v.d.str); char* s = (char*)malloc(l + 1);\n"

             << "    for (size_t i = 0; i <= l; i++) s[i] = (char)tolower((unsigned char)v.d.str[i]);\n"

             << "    RqValue r; r.type = RQ_STR; r.d.str = s; return r;\n"

             << "}\n"

             << "static inline RqValue rq_trim(RqValue v) {\n"

             << "    if (v.type != RQ_STR || !v.d.str) return rq_str_lit(\"\");\n"

             << "    const char* start = v.d.str;\n"

             << "    while (*start && isspace((unsigned char)*start)) start++;\n"

             << "    const char* end = v.d.str + strlen(v.d.str);\n"

             << "    while (end > start && isspace((unsigned char)*(end - 1))) end--;\n"

             << "    size_t l = (size_t)(end - start); char* s = (char*)malloc(l + 1);\n"

             << "    memcpy(s, start, l); s[l] = 0;\n"

             << "    RqValue r; r.type = RQ_STR; r.d.str = s; return r;\n"

             << "}\n"

             << "static inline RqValue rq_replace(RqValue strVal, RqValue fromVal, RqValue toVal) {\n"

             << "    if (strVal.type != RQ_STR || fromVal.type != RQ_STR || toVal.type != RQ_STR) return strVal;\n"

             << "    const char* src = strVal.d.str ? strVal.d.str : \"\";\n"

             << "    const char* f = fromVal.d.str ? fromVal.d.str : \"\";\n"

             << "    const char* t = toVal.d.str ? toVal.d.str : \"\";\n"

             << "    size_t fl = strlen(f); if (fl == 0) return strVal;\n"

             << "    size_t tl = strlen(t), count = 0; const char* p = src;\n"

             << "    while ((p = strstr(p, f)) != NULL) { count++; p += fl; }\n"

             << "    size_t sl = strlen(src), newLen = sl + count * (tl - fl);\n"

             << "    char* res = (char*)malloc(newLen + 1); char* dst = res; p = src;\n"

             << "    while (*p) {\n"

             << "        if (strncmp(p, f, fl) == 0) { memcpy(dst, t, tl); dst += tl; p += fl; }\n"

             << "        else { *dst++ = *p++; }\n"

             << "    }\n"

             << "    *dst = 0; RqValue r; r.type = RQ_STR; r.d.str = res; return r;\n"

             << "}\n"

             << "static inline RqValue rq_slice(RqValue target, RqValue startVal, RqValue endVal) {\n"

             << "    int start = (int)startVal.d.num;\n"

             << "    if (target.type == RQ_STR && target.d.str) {\n"

             << "        int len = (int)strlen(target.d.str);\n"

             << "        int end = (endVal.type == RQ_NULL) ? len : (int)endVal.d.num;\n"

             << "        if (start < 0) start = 0; if (end > len) end = len;\n"

             << "        if (start >= end) return rq_str_lit(\"\");\n"

             << "        int sl = end - start; char* s = (char*)malloc(sl + 1);\n"

             << "        memcpy(s, target.d.str + start, sl); s[sl] = 0;\n"

             << "        RqValue r; r.type = RQ_STR; r.d.str = s; return r;\n"

             << "    }\n"

             << "    if (target.type == RQ_ARR && target.d.arr) {\n"

             << "        int len = target.d.arr->size;\n"

             << "        int end = (endVal.type == RQ_NULL) ? len : (int)endVal.d.num;\n"

             << "        if (start < 0) start = 0; if (end > len) end = len;\n"

             << "        int count = (end > start) ? (end - start) : 0;\n"

             << "        RqValue res = rq_arr_new(count);\n"

             << "        for (int i = start; i < end; i++) rq_push(res, target.d.arr->items[i]);\n"

             << "        return res;\n"

             << "    }\n"

             << "    return target;\n"

             << "}\n"

             << "static inline RqValue rq_range(RqValue startVal, RqValue endVal) {\n"

             << "    int start = (int)startVal.d.num, end = (int)endVal.d.num;\n"

             << "    int count = end > start ? (end - start) : 0;\n"

             << "    RqValue arr = rq_arr_new(count);\n"

             << "    for (int i = start; i < end; i++) rq_push(arr, rq_num((double)i));\n"

             << "    return arr;\n"

             << "}\n"

             << "static inline RqValue rq_range_inclusive(RqValue startVal, RqValue endVal) {\n"

             << "    int start = (int)startVal.d.num, end = (int)endVal.d.num;\n"

             << "    int count = end >= start ? (end - start + 1) : 0;\n"

             << "    RqValue arr = rq_arr_new(count);\n"

             << "    for (int i = start; i <= end; i++) rq_push(arr, rq_num((double)i));\n"

             << "    return arr;\n"

             << "}\n"

             << "static inline RqValue rq_join(RqValue arrVal, RqValue sepVal) {\n"

             << "    if (arrVal.type != RQ_ARR || !arrVal.d.arr) return rq_str_lit(\"\");\n"

             << "    const char* sep = (sepVal.type == RQ_STR && sepVal.d.str) ? sepVal.d.str : \"\";\n"

             << "    size_t seplen = strlen(sep), total = 0; int count = arrVal.d.arr->size;\n"

             << "    char** items = (char**)malloc(sizeof(char*) * (size_t)(count > 0 ? count : 1));\n"

             << "    for (int i = 0; i < count; i++) { items[i] = rq_to_cstr(arrVal.d.arr->items[i]); total += strlen(items[i]); }\n"

             << "    if (count > 1) total += seplen * (count - 1);\n"

             << "    char* res = (char*)malloc(total + 1); res[0] = 0;\n"

             << "    for (int i = 0; i < count; i++) {\n"

             << "        if (i > 0 && seplen > 0) strcat(res, sep);\n"

             << "        strcat(res, items[i]); free(items[i]);\n"

             << "    }\n"

             << "    free(items); RqValue r; r.type = RQ_STR; r.d.str = res; return r;\n"

             << "}\n"

             << "static inline RqValue rq_repeat(RqValue sVal, RqValue countVal) {\n"

             << "    if (sVal.type != RQ_STR || !sVal.d.str) return rq_str_lit(\"\");\n"

             << "    int count = (int)countVal.d.num; if (count <= 0) return rq_str_lit(\"\");\n"

             << "    size_t sl = strlen(sVal.d.str); char* res = (char*)malloc(sl * count + 1); res[0] = 0;\n"

             << "    for (int i = 0; i < count; i++) strcat(res, sVal.d.str);\n"

             << "    RqValue r; r.type = RQ_STR; r.d.str = res; return r;\n"

             << "}\n"

             << "static inline RqValue rq_char(RqValue numVal) {\n"

             << "    char buf[2] = { (char)((int)numVal.d.num), 0 }; return rq_str_copy(buf);\n"

             << "}\n\n"

             << "static inline RqValue rq_print(RqValue v){\n"

             << "    if(v.type==RQ_STR&&v.d.str){printf(\"%s\\n\",v.d.str);}\n"

             << "    else{char*s=rq_to_cstr(v);printf(\"%s\\n\",s);free(s);}\n"

             << "    return rq_null();}\n"

             << "static inline void rq_throw_msg(const char* msg){\n"

             << "    _rq_last_error = rq_str_copy(msg ? msg : \"\");\n"

             << "    if (_rq_catch_top) { longjmp(_rq_catch_top->jbuf, 1); }\n"

             << "    else { fprintf(stderr, \"error: %s\\n\", msg ? msg : \"\"); exit(1); }\n"

             << "}\n"

             << "static inline RqValue rq_assert(RqValue c){if(!rq_truthy(c)){rq_throw_msg(\"Assertion failed\");}return rq_bool(1);}\n"

             << "static inline RqValue rq_assert_true(RqValue c){return rq_assert(c);}\n"

             << "static inline RqValue rq_assert_false(RqValue c){if(rq_truthy(c)){rq_throw_msg(\"Assertion failed: expected false\");}return rq_bool(1);}\n"

             << "static inline RqValue rq_assert_null(RqValue c){if(c.type!=RQ_NULL){rq_throw_msg(\"Assertion failed: expected null\");}return rq_bool(1);}\n"

             << "static inline RqValue rq_assert_eq(RqValue a,RqValue b){\n"

             << "    if(!rq_truthy(rq_eq(a,b))){\n"

             << "        char*sa=rq_to_cstr(a);char*sb=rq_to_cstr(b);\n"

             << "        char buf[512]; snprintf(buf,sizeof(buf),\"Assertion failed: expected %s, got %s\",sb,sa);\n"

             << "        free(sa);free(sb);rq_throw_msg(buf);}\n"

             << "    return rq_bool(1);}\n"

             << "static inline RqValue rq_assert_ne(RqValue a,RqValue b){if(rq_truthy(rq_eq(a,b))){rq_throw_msg(\"Assertion failed: values are equal\");}return rq_bool(1);}\n"

             << "static inline RqValue rq_str_builtin(RqValue v){return rq_to_str(v);}\n"

             << "static inline RqValue rq_num_builtin(RqValue v){\n"

             << "    switch(v.type){case RQ_NUM:return v;case RQ_BOOL:return rq_num(v.d.b?1.0:0.0);\n"

             << "    case RQ_STR:return rq_num(atof(v.d.str));default:return rq_num(0.0);}}\n"

             << "static inline RqValue rq_bool_builtin(RqValue v){return rq_bool(rq_truthy(v));}\n"

             << "static inline RqValue rq_type_builtin(RqValue v){\n"

             << "    switch(v.type){case RQ_NULL:return rq_str_copy(\"null\");case RQ_NUM:return rq_str_copy(\"number\");\n"

             << "    case RQ_STR:return rq_str_copy(\"string\");case RQ_BOOL:return rq_str_copy(\"bool\");\n"

             << "    default:return rq_str_copy(\"unknown\");}}\n"

             << "static inline RqValue rq_sqrt(RqValue v){return rq_num(sqrt(v.d.num));}\n"

             << "static inline RqValue rq_abs(RqValue v){return rq_num(fabs(v.d.num));}\n"

             << "static inline RqValue rq_floor(RqValue v){return rq_num(floor(v.d.num));}\n"

             << "static inline RqValue rq_ceil(RqValue v){return rq_num(ceil(v.d.num));}\n"

             << "static inline RqValue rq_round(RqValue v){return rq_num(round(v.d.num));}\n"

             << "static inline RqValue rq_pow(RqValue a,RqValue b){return rq_num(pow(a.d.num,b.d.num));}\n"

             << "static inline RqValue rq_concat_n(RqValue* parts,int n){\n"

             << "    if(n<=0)return rq_str_copy(\"\");\n"

             << "    size_t total=0;\n"

             << "    char** ptrs=(char**)malloc((size_t)n*sizeof(char*));\n"

             << "    for(int i=0;i<n;++i){ptrs[i]=rq_to_cstr(parts[i]);total+=strlen(ptrs[i]);}\n"

             << "    char* buf=(char*)malloc(total+1);buf[0]=0;\n"

             << "    for(int i=0;i<n;++i){strcat(buf,ptrs[i]);free(ptrs[i]);}\n"

             << "    free(ptrs);\n"
             << "    RqValue r;r.type=RQ_STR;r.d.str=buf;return r;\n"
             << "}\n\n"
             << "static inline RqValue rq_clamp(RqValue val, RqValue minVal, RqValue maxVal) {\n"
             << "    double v = val.d.num, mn = minVal.d.num, mx = maxVal.d.num;\n"
             << "    if (v < mn) v = mn; if (v > mx) v = mx; return rq_num(v);\n"
             << "}\n\n"
             << "static inline RqValue rq_vec2(RqValue x, RqValue y) {\n"
             << "    RqValue v = rq_obj_new(4); rq_obj_set(v, \"x\", x); rq_obj_set(v, \"y\", y);\n"
             << "    rq_obj_set(v, \"__type__\", rq_str_lit(\"vec2\")); return v;\n"
             << "}\n\n"
             << "static inline RqValue rq_vec3(RqValue x, RqValue y, RqValue z) {\n"
             << "    RqValue v = rq_obj_new(4); rq_obj_set(v, \"x\", x); rq_obj_set(v, \"y\", y); rq_obj_set(v, \"z\", z);\n"
             << "    rq_obj_set(v, \"__type__\", rq_str_lit(\"vec3\")); return v;\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_add(RqValue a, RqValue b) {\n"
             << "    RqValue ax = rq_obj_get(a, \"x\"), ay = rq_obj_get(a, \"y\"), az = rq_obj_get(a, \"z\");\n"
             << "    RqValue bx = rq_obj_get(b, \"x\"), by = rq_obj_get(b, \"y\"), bz = rq_obj_get(b, \"z\");\n"
             << "    if (az.type != RQ_NULL || bz.type != RQ_NULL) return rq_vec3(rq_num(ax.d.num + bx.d.num), rq_num(ay.d.num + by.d.num), rq_num(az.d.num + bz.d.num));\n"
             << "    return rq_vec2(rq_num(ax.d.num + bx.d.num), rq_num(ay.d.num + by.d.num));\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_sub(RqValue a, RqValue b) {\n"
             << "    RqValue ax = rq_obj_get(a, \"x\"), ay = rq_obj_get(a, \"y\"), az = rq_obj_get(a, \"z\");\n"
             << "    RqValue bx = rq_obj_get(b, \"x\"), by = rq_obj_get(b, \"y\"), bz = rq_obj_get(b, \"z\");\n"
             << "    if (az.type != RQ_NULL || bz.type != RQ_NULL) return rq_vec3(rq_num(ax.d.num - bx.d.num), rq_num(ay.d.num - by.d.num), rq_num(az.d.num - bz.d.num));\n"
             << "    return rq_vec2(rq_num(ax.d.num - bx.d.num), rq_num(ay.d.num - by.d.num));\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_mul(RqValue a, RqValue s) {\n"
             << "    RqValue ax = rq_obj_get(a, \"x\"), ay = rq_obj_get(a, \"y\"), az = rq_obj_get(a, \"z\");\n"
             << "    double scale = s.d.num;\n"
             << "    if (az.type != RQ_NULL) return rq_vec3(rq_num(ax.d.num * scale), rq_num(ay.d.num * scale), rq_num(az.d.num * scale));\n"
             << "    return rq_vec2(rq_num(ax.d.num * scale), rq_num(ay.d.num * scale));\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_dot(RqValue a, RqValue b) {\n"
             << "    RqValue ax = rq_obj_get(a, \"x\"), ay = rq_obj_get(a, \"y\"), az = rq_obj_get(a, \"z\");\n"
             << "    RqValue bx = rq_obj_get(b, \"x\"), by = rq_obj_get(b, \"y\"), bz = rq_obj_get(b, \"z\");\n"
             << "    double d = ax.d.num * bx.d.num + ay.d.num * by.d.num;\n"
             << "    if (az.type != RQ_NULL && bz.type != RQ_NULL) d += az.d.num * bz.d.num;\n"
             << "    return rq_num(d);\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_len(RqValue v) {\n"
             << "    RqValue x = rq_obj_get(v, \"x\"), y = rq_obj_get(v, \"y\"), z = rq_obj_get(v, \"z\");\n"
             << "    double sum = x.d.num * x.d.num + y.d.num * y.d.num;\n"
             << "    if (z.type != RQ_NULL) sum += z.d.num * z.d.num;\n"
             << "    return rq_num(sqrt(sum));\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_normalize(RqValue v) {\n"
             << "    double l = rq_vec_len(v).d.num;\n"
             << "    if (l < 1e-12) l = 1.0;\n"
             << "    return rq_vec_mul(v, rq_num(1.0 / l));\n"
             << "}\n\n"
             << "static inline RqValue rq_vec_cross(RqValue a, RqValue b) {\n"
             << "    double ax = rq_obj_get(a, \"x\").d.num, ay = rq_obj_get(a, \"y\").d.num, az = rq_obj_get(a, \"z\").d.num;\n"
             << "    double bx = rq_obj_get(b, \"x\").d.num, by = rq_obj_get(b, \"y\").d.num, bz = rq_obj_get(b, \"z\").d.num;\n"
             << "    return rq_vec3(rq_num(ay * bz - az * by), rq_num(az * bx - ax * bz), rq_num(ax * by - ay * bx));\n"
             << "}\n\n"
             << "static void _rq_json_append(char** buf, size_t* len, size_t* cap, const char* str) {\n"
             << "    size_t slen = strlen(str);\n"
             << "    if (*len + slen + 1 > *cap) { *cap = (*len + slen + 1) * 2 + 128; *buf = (char*)realloc(*buf, *cap); }\n"
             << "    memcpy(*buf + *len, str, slen); *len += slen; (*buf)[*len] = 0;\n"
             << "}\n\n"
             << "static void _rq_json_stringify_rec(RqValue v, char** buf, size_t* len, size_t* cap) {\n"
             << "    if (v.type == RQ_NULL) _rq_json_append(buf, len, cap, \"null\");\n"
             << "    else if (v.type == RQ_BOOL) _rq_json_append(buf, len, cap, v.d.b ? \"true\" : \"false\");\n"
             << "    else if (v.type == RQ_NUM) {\n"
             << "        char nbuf[64];\n"
             << "        if (v.d.num == (long long)v.d.num && v.d.num >= -1e15 && v.d.num <= 1e15) snprintf(nbuf, sizeof(nbuf), \"%lld\", (long long)v.d.num);\n"
             << "        else snprintf(nbuf, sizeof(nbuf), \"%g\", v.d.num);\n"
             << "        _rq_json_append(buf, len, cap, nbuf);\n"
             << "    } else if (v.type == RQ_STR) {\n"
             << "        _rq_json_append(buf, len, cap, \"\\\"\");\n"
             << "        if (v.d.str) {\n"
             << "            for (const char* p = v.d.str; *p; ++p) {\n"
             << "                if (*p == '\"') _rq_json_append(buf, len, cap, \"\\\\\\\"\");\n"
             << "                else if (*p == '\\\\') _rq_json_append(buf, len, cap, \"\\\\\\\\\");\n"
             << "                else if (*p == '\\n') _rq_json_append(buf, len, cap, \"\\\\n\");\n"
             << "                else if (*p == '\\r') _rq_json_append(buf, len, cap, \"\\\\r\");\n"
             << "                else if (*p == '\\t') _rq_json_append(buf, len, cap, \"\\\\t\");\n"
             << "                else { char ch[2] = {*p, 0}; _rq_json_append(buf, len, cap, ch); }\n"
             << "            }\n"
             << "        }\n"
             << "        _rq_json_append(buf, len, cap, \"\\\"\");\n"
             << "    } else if (v.type == RQ_ARR) {\n"
             << "        _rq_json_append(buf, len, cap, \"[\");\n"
             << "        if (v.d.arr) {\n"
             << "            for (int i = 0; i < v.d.arr->size; ++i) {\n"
             << "                if (i > 0) _rq_json_append(buf, len, cap, \",\");\n"
             << "                _rq_json_stringify_rec(v.d.arr->items[i], buf, len, cap);\n"
             << "            }\n"
             << "        }\n"
             << "        _rq_json_append(buf, len, cap, \"]\");\n"
             << "    } else if (v.type == RQ_OBJ) {\n"
             << "        _rq_json_append(buf, len, cap, \"{\");\n"
             << "        if (v.d.obj) {\n"
             << "            int count = 0;\n"
             << "            for (int i = 0; i < v.d.obj->size; ++i) {\n"
             << "                if (strcmp(v.d.obj->entries[i].key, \"__type__\") == 0) continue;\n"
             << "                if (count > 0) _rq_json_append(buf, len, cap, \",\");\n"
             << "                _rq_json_append(buf, len, cap, \"\\\"\");\n"
             << "                _rq_json_append(buf, len, cap, v.d.obj->entries[i].key);\n"
             << "                _rq_json_append(buf, len, cap, \"\\\":\");\n"
             << "                _rq_json_stringify_rec(v.d.obj->entries[i].val, buf, len, cap);\n"
             << "                count++;\n"
             << "            }\n"
             << "        }\n"
             << "        _rq_json_append(buf, len, cap, \"}\");\n"
             << "    }\n"
             << "}\n\n"
             << "static inline RqValue rq_json_stringify(RqValue v) {\n"
             << "    size_t len = 0, cap = 256;\n"
             << "    char* buf = (char*)malloc(cap); buf[0] = 0;\n"
             << "    _rq_json_stringify_rec(v, &buf, &len, &cap);\n"
             << "    RqValue r; r.type = RQ_STR; r.d.str = buf; return r;\n"
             << "}\n\n"
             << "static inline void _rq_json_skip_ws(const char** p) {\n"
             << "    while (**p && (**p == ' ' || **p == '\\t' || **p == '\\n' || **p == '\\r')) (*p)++;\n"
             << "}\n\n"
             << "static RqValue _rq_json_parse_val(const char** p);\n"
             << "static RqValue _rq_json_parse_str(const char** p) {\n"
             << "    (*p)++; size_t cap = 32, len = 0; char* buf = (char*)malloc(cap);\n"
             << "    while (**p && **p != '\"') {\n"
             << "        char c = **p;\n"
             << "        if (c == 92 && *(*p + 1)) { (*p)++; if (**p == 'n') c = 10; else if (**p == 'r') c = 13; else if (**p == 't') c = 9; else c = **p; }\n"
             << "        if (len + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }\n"
             << "        buf[len++] = c; (*p)++;\n"
             << "    }\n"
             << "    if (**p == '\"') (*p)++; buf[len] = 0; RqValue r; r.type = RQ_STR; r.d.str = buf; return r;\n"
             << "}\n\n"
             << "static RqValue _rq_json_parse_val(const char** p) {\n"
             << "    _rq_json_skip_ws(p);\n"
             << "    if (!**p) return rq_null();\n"
             << "    if (**p == '\"') return _rq_json_parse_str(p);\n"
             << "    if (**p == '{') {\n"
             << "        (*p)++; RqValue obj = rq_obj_new(8); _rq_json_skip_ws(p);\n"
             << "        if (**p == '}') { (*p)++; return obj; }\n"
             << "        while (**p) {\n"
             << "            _rq_json_skip_ws(p); if (**p != '\"') break;\n"
             << "            RqValue k = _rq_json_parse_str(p); _rq_json_skip_ws(p);\n"
             << "            if (**p == ':') (*p)++;\n"
             << "            RqValue val = _rq_json_parse_val(p); rq_obj_set(obj, k.d.str, val); free(k.d.str);\n"
             << "            _rq_json_skip_ws(p); if (**p == ',') { (*p)++; continue; } if (**p == '}') { (*p)++; break; }\n"
             << "        }\n"
             << "        return obj;\n"
             << "    }\n"
             << "    if (**p == '[') {\n"
             << "        (*p)++; RqValue arr = rq_arr_new(8); _rq_json_skip_ws(p);\n"
             << "        if (**p == ']') { (*p)++; return arr; }\n"
             << "        while (**p) {\n"
             << "            RqValue item = _rq_json_parse_val(p); rq_push(arr, item);\n"
             << "            _rq_json_skip_ws(p); if (**p == ',') { (*p)++; continue; } if (**p == ']') { (*p)++; break; }\n"
             << "        }\n"
             << "        return arr;\n"
             << "    }\n"
             << "    if (strncmp(*p, \"true\", 4) == 0) { *p += 4; return rq_bool(1); }\n"
             << "    if (strncmp(*p, \"false\", 5) == 0) { *p += 5; return rq_bool(0); }\n"
             << "    if (strncmp(*p, \"null\", 4) == 0) { *p += 4; return rq_null(); }\n"
             << "    char* endptr = NULL; double d = strtod(*p, &endptr);\n"
             << "    if (endptr != *p) { *p = endptr; return rq_num(d); }\n"
             << "    return rq_null();\n"
             << "}\n\n"
             << "static inline RqValue rq_json_parse(RqValue s) {\n"
             << "    if (s.type != RQ_STR || !s.d.str) return rq_null();\n"
             << "    const char* p = s.d.str; return _rq_json_parse_val(&p);\n"
             << "}\n\n"
             << "static inline RqValue rq_sha256(RqValue v) {\n"
             << "    char* src = rq_to_cstr(v);\n"
             << "    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;\n"
             << "    uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;\n"
             << "    static const uint32_t k[64] = {\n"
             << "        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,\n"
             << "        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,\n"
             << "        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,\n"
             << "        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,\n"
             << "        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,\n"
             << "        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,\n"
             << "        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,\n"
             << "        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2\n"
             << "    };\n"
             << "    size_t slen = strlen(src); size_t new_len = ((((slen + 8) / 64) + 1) * 64);\n"
             << "    uint8_t* p = (uint8_t*)calloc(new_len, 1); memcpy(p, src, slen); p[slen] = 0x80;\n"
             << "    uint64_t bits = (uint64_t)slen * 8;\n"
             << "    for (int i = 0; i < 8; i++) p[new_len - 1 - i] = (uint8_t)(bits >> (i * 8));\n"
             << "    for (size_t chunk = 0; chunk < new_len; chunk += 64) {\n"
             << "        uint32_t w[64];\n"
             << "        for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[chunk + i*4] << 24) | ((uint32_t)p[chunk + i*4 + 1] << 16) | ((uint32_t)p[chunk + i*4 + 2] << 8) | ((uint32_t)p[chunk + i*4 + 3]);\n"
             << "        for (int i = 16; i < 64; i++) {\n"
             << "            uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25)) ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);\n"
             << "            uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15)) ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);\n"
             << "            w[i] = w[i-16] + s0 + w[i-7] + s1;\n"
             << "        }\n"
             << "        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;\n"
             << "        for (int i = 0; i < 64; i++) {\n"
             << "            uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));\n"
             << "            uint32_t ch = (e & f) ^ ((~e) & g);\n"
             << "            uint32_t temp1 = h + S1 + ch + k[i] + w[i];\n"
             << "            uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));\n"
             << "            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);\n"
             << "            uint32_t temp2 = S0 + maj;\n"
             << "            h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;\n"
             << "        }\n"
             << "        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;\n"
             << "    }\n"
             << "    free(p); free(src); char out_hex[65];\n"
             << "    snprintf(out_hex, sizeof(out_hex), \"%08x%08x%08x%08x%08x%08x%08x%08x\", h0, h1, h2, h3, h4, h5, h6, h7);\n"
             << "    return rq_str_copy(out_hex);\n"
             << "}\n\n"
             << "static inline RqValue rq_base64_encode(RqValue v) {\n"
             << "    char* src = rq_to_cstr(v); size_t in_len = strlen(src);\n"
             << "    static const char table[] = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\";\n"
             << "    size_t out_len = 4 * ((in_len + 2) / 3); char* out = (char*)malloc(out_len + 1); out[out_len] = 0;\n"
             << "    for (size_t i = 0, j = 0; i < in_len;) {\n"
             << "        uint32_t oct_a = i < in_len ? (unsigned char)src[i++] : 0;\n"
             << "        uint32_t oct_b = i < in_len ? (unsigned char)src[i++] : 0;\n"
             << "        uint32_t oct_c = i < in_len ? (unsigned char)src[i++] : 0;\n"
             << "        uint32_t triple = (oct_a << 16) + (oct_b << 8) + oct_c;\n"
             << "        out[j++] = table[(triple >> 18) & 0x3F]; out[j++] = table[(triple >> 12) & 0x3F];\n"
             << "        out[j++] = i > in_len + 1 ? '=' : table[(triple >> 6) & 0x3F];\n"
             << "        out[j++] = i > in_len ? '=' : table[triple & 0x3F];\n"
             << "    }\n"
             << "    free(src); RqValue r; r.type = RQ_STR; r.d.str = out; return r;\n"
             << "}\n\n"
             << "static inline int _rq_b64_dval(char c) {\n"
             << "    if (c >= 'A' && c <= 'Z') return c - 'A'; if (c >= 'a' && c <= 'z') return c - 'a' + 26;\n"
             << "    if (c >= '0' && c <= '9') return c - '0' + 52; if (c == '+') return 62; if (c == '/') return 63; return 0;\n"
             << "}\n\n"
             << "static inline RqValue rq_base64_decode(RqValue v) {\n"
             << "    char* src = rq_to_cstr(v); size_t in_len = strlen(src);\n"
             << "    if (in_len % 4 != 0) { free(src); return rq_str_copy(\"\"); }\n"
             << "    size_t out_len = in_len / 4 * 3;\n"
             << "    if (in_len > 0 && src[in_len - 1] == '=') out_len--;\n"
             << "    if (in_len > 1 && src[in_len - 2] == '=') out_len--;\n"
             << "    char* out = (char*)malloc(out_len + 1); out[out_len] = 0;\n"
             << "    for (size_t i = 0, j = 0; i < in_len;) {\n"
             << "        uint32_t a = src[i] == '=' ? 0 : _rq_b64_dval(src[i]); i++;\n"
             << "        uint32_t b = src[i] == '=' ? 0 : _rq_b64_dval(src[i]); i++;\n"
             << "        uint32_t c = src[i] == '=' ? 0 : _rq_b64_dval(src[i]); i++;\n"
             << "        uint32_t d = src[i] == '=' ? 0 : _rq_b64_dval(src[i]); i++;\n"
             << "        uint32_t triple = (a << 18) + (b << 12) + (c << 6) + d;\n"
             << "        if (j < out_len) out[j++] = (triple >> 16) & 0xFF;\n"
             << "        if (j < out_len) out[j++] = (triple >> 8) & 0xFF;\n"
             << "        if (j < out_len) out[j++] = triple & 0xFF;\n"
             << "    }\n"
             << "    free(src); RqValue r; r.type = RQ_STR; r.d.str = out; return r;\n"
             << "}\n\n"
             << "static inline RqValue rq_uuid(void) {\n"
             << "    char buf[37]; static uint32_t seed = 0x12345678;\n"
             << "    seed = seed * 1664525u + 1013904223u; uint32_t r1 = seed;\n"
             << "    seed = seed * 1664525u + 1013904223u; uint32_t r2 = seed;\n"
             << "    seed = seed * 1664525u + 1013904223u; uint32_t r3 = seed;\n"
             << "    seed = seed * 1664525u + 1013904223u; uint32_t r4 = seed;\n"
             << "    snprintf(buf, sizeof(buf), \"%08x-%04x-4%03x-%04x-%04x%08x\", r1, r2 & 0xffff, r2 & 0x0fff, (r3 & 0x3fff) | 0x8000, r3 & 0xffff, r4);\n"
             << "    return rq_str_copy(buf);\n"
             << "}\n\n"
             << "static inline RqValue rq_process_cwd(void) {\n"
             << "    char buf[1024];\n"
             << "#ifdef _WIN32\n"
             << "    if (GetCurrentDirectoryA(sizeof(buf), buf)) return rq_str_copy(buf);\n"
             << "#else\n"
             << "    if (getcwd(buf, sizeof(buf))) return rq_str_copy(buf);\n"
             << "#endif\n"
             << "    return rq_str_copy(\".\");\n"
             << "}\n\n"
             << "static inline RqValue rq_path_join(RqValue a, RqValue b) {\n"
             << "    char* sa = rq_to_cstr(a); char* sb = rq_to_cstr(b);\n"
             << "    size_t la = strlen(sa), lb = strlen(sb); char* res = (char*)malloc(la + lb + 2); strcpy(res, sa);\n"
             << "    if (la > 0 && res[la - 1] != '/' && res[la - 1] != '\\\\') { res[la] = '/'; res[la + 1] = 0; }\n"
             << "    strcat(res, sb); free(sa); free(sb); RqValue r; r.type = RQ_STR; r.d.str = res; return r;\n"
             << "}\n\n"
             << "static inline RqValue rq_path_basename(RqValue p) {\n"
             << "    char* sp = rq_to_cstr(p); const char* base = sp;\n"
             << "    for (const char* s = sp; *s; s++) { if (*s == '/' || *s == '\\\\') base = s + 1; }\n"
             << "    RqValue r = rq_str_copy(base); free(sp); return r;\n"
             << "}\n\n"
             << "static inline RqValue rq_path_ext(RqValue p) {\n"
             << "    char* sp = rq_to_cstr(p); const char* dot = NULL;\n"
             << "    for (const char* s = sp; *s; s++) { if (*s == '.') dot = s; else if (*s == '/' || *s == '\\\\') dot = NULL; }\n"
             << "    RqValue r = rq_str_copy(dot ? dot : \"\"); free(sp); return r;\n"
             << "}\n\n"
             << "static inline RqValue rq_fs_write(RqValue p, RqValue data) {\n"
             << "    char* sp = rq_to_cstr(p); char* sd = rq_to_cstr(data);\n"
             << "    FILE* f = fopen(sp, \"wb\"); if (f) { fwrite(sd, 1, strlen(sd), f); fclose(f); }\n"
             << "    free(sp); free(sd); return rq_bool(f != NULL);\n"
             << "}\n\n"
             << "static inline RqValue rq_fs_read(RqValue p) {\n"
             << "    char* sp = rq_to_cstr(p); FILE* f = fopen(sp, \"rb\");\n"
             << "    if (!f) { free(sp); return rq_str_copy(\"\"); }\n"
             << "    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);\n"
             << "    char* buf = (char*)malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = 0; fclose(f); free(sp);\n"
             << "    RqValue r; r.type = RQ_STR; r.d.str = buf; return r;\n"
             << "}\n\n"
             << "static inline RqValue rq_fs_exists(RqValue p) {\n"
             << "    char* sp = rq_to_cstr(p); FILE* f = fopen(sp, \"rb\");\n"
             << "    if (f) { fclose(f); free(sp); return rq_bool(1); }\n"
             << "    free(sp); return rq_bool(0);\n"
             << "}\n\n"
             << "static inline RqValue rq_fs_remove(RqValue p) {\n"
             << "    char* sp = rq_to_cstr(p); int res = remove(sp); free(sp); return rq_bool(res == 0);\n"
             << "}\n\n"
             << "static inline RqValue rq_env_set(RqValue k, RqValue v) {\n"
             << "    char* sk = rq_to_cstr(k); char* sv = rq_to_cstr(v);\n"
             << "#ifdef _WIN32\n"
             << "    _putenv_s(sk, sv); SetEnvironmentVariableA(sk, sv);\n"
             << "#else\n"
             << "    setenv(sk, sv, 1);\n"
             << "#endif\n"
             << "    free(sk); free(sv); return rq_null();\n"
             << "}\n\n"
             << "static inline RqValue rq_env_get(RqValue k) {\n"
             << "    char* sk = rq_to_cstr(k); const char* val = getenv(sk); free(sk); return val ? rq_str_copy(val) : rq_null();\n"
             << "}\n\n"
             << "static inline RqValue rq_env_has(RqValue k) {\n"
             << "    char* sk = rq_to_cstr(k); const char* val = getenv(sk); free(sk); return rq_bool(val != NULL);\n"
             << "}\n\n"
             << "static inline RqValue rq_ui_dark(void) {\n"
             << "    RqValue obj = rq_obj_new(8); rq_obj_set(obj, \"name\", rq_str_lit(\"dark\")); rq_obj_set(obj, \"primary\", rq_str_lit(\"#3b82f6\"));\n"
             << "    rq_obj_set(obj, \"background\", rq_str_lit(\"#18181b\")); rq_obj_set(obj, \"surface\", rq_str_lit(\"#27272a\")); return obj;\n"
             << "}\n\n"
             << "static inline RqValue rq_ui_rgb(RqValue r, RqValue g, RqValue b) {\n"
             << "    char buf[16]; snprintf(buf, sizeof(buf), \"#%02x%02x%02x\", (int)r.d.num & 0xFF, (int)g.d.num & 0xFF, (int)b.d.num & 0xFF); return rq_str_copy(buf);\n"
             << "}\n\n"
             << "static inline RqValue rq_ui_blend(RqValue c1, RqValue c2, RqValue t) {\n"
             << "    char* s1 = rq_to_cstr(c1); char* s2 = rq_to_cstr(c2);\n"
             << "    int r1 = 0, g1 = 0, b1 = 0, r2 = 0, g2 = 0, b2 = 0;\n"
             << "    if (s1[0] == '#') sscanf(s1 + 1, \"%02x%02x%02x\", &r1, &g1, &b1);\n"
             << "    if (s2[0] == '#') sscanf(s2 + 1, \"%02x%02x%02x\", &r2, &g2, &b2);\n"
             << "    double factor = t.d.num; int r = (int)(r1 + (r2 - r1) * factor); int g = (int)(g1 + (g2 - g1) * factor); int b = (int)(b1 + (b2 - b1) * factor);\n"
             << "    free(s1); free(s2); char buf[16]; snprintf(buf, sizeof(buf), \"#%02x%02x%02x\", r & 0xFF, g & 0xFF, b & 0xFF); return rq_str_copy(buf);\n"
             << "}\n\n"
             << "static inline RqValue rq_app_button(RqValue text, RqValue opts) {\n"
             << "    RqValue obj = rq_obj_new(8); rq_obj_set(obj, \"tag\", rq_str_lit(\"button\")); rq_obj_set(obj, \"text\", text);\n"
             << "    if (opts.type == RQ_OBJ) { RqValue idVal = rq_obj_get(opts, \"id\"); if (idVal.type != RQ_NULL) rq_obj_set(obj, \"id\", idVal); } return obj;\n"
             << "}\n\n"
             << "static inline RqValue rq_app_text(RqValue text) {\n"
             << "    RqValue obj = rq_obj_new(4); rq_obj_set(obj, \"tag\", rq_str_lit(\"text\")); rq_obj_set(obj, \"text\", text); return obj;\n"
             << "}\n\n"
             << "static inline RqValue rq_app_column(RqValue children) {\n"
             << "    RqValue obj = rq_obj_new(4); rq_obj_set(obj, \"tag\", rq_str_lit(\"column\")); rq_obj_set(obj, \"children\", children); return obj;\n"
             << "}\n\n"
             << "static inline RqValue rq_app_to_html(RqValue node) { return rq_str_lit(\"<div>RayQuiro Native HTML</div>\"); }\n\n"
             << "static inline RqValue rq_engine_capabilities(void) {\n"
             << "    RqValue obj = rq_obj_new(8); rq_obj_set(obj, \"supports3D\", rq_bool(1)); rq_obj_set(obj, \"supportsPbr\", rq_bool(1));\n"
             << "    rq_obj_set(obj, \"supportsRaycasting\", rq_bool(1)); rq_obj_set(obj, \"supportsMultiLights\", rq_bool(1)); rq_obj_set(obj, \"maxLights\", rq_num(8.0)); return obj;\n"
             << "}\n\n"
             << "typedef struct RqEngineEntity { char name[64]; char kind[64]; double pos[3]; double sz[3]; char tags[8][32]; int tag_count; } RqEngineEntity;\n"
             << "static RqEngineEntity _rq_entities[64]; static int _rq_entity_count = 0;\n"
             << "static inline RqValue _rq_entity_to_val(const RqEngineEntity* e) {\n"
             << "    RqValue obj = rq_obj_new(8); rq_obj_set(obj, \"name\", rq_str_copy(e->name)); rq_obj_set(obj, \"kind\", rq_str_copy(e->kind));\n"
             << "    RqValue pArr = rq_arr_new(3); rq_push(pArr, rq_num(e->pos[0])); rq_push(pArr, rq_num(e->pos[1])); rq_push(pArr, rq_num(e->pos[2])); rq_obj_set(obj, \"position\", pArr); return obj;\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_entity(RqValue nameVal, RqValue kindVal, RqValue posVal, RqValue szVal) {\n"
             << "    if (_rq_entity_count >= 64) return rq_null(); char* n = rq_to_cstr(nameVal); char* k = rq_to_cstr(kindVal);\n"
             << "    RqEngineEntity* e = &_rq_entities[_rq_entity_count++]; strncpy(e->name, n, 63); strncpy(e->kind, k, 63);\n"
             << "    e->pos[0] = rq_obj_get(posVal, \"x\").d.num; e->pos[1] = rq_obj_get(posVal, \"y\").d.num; e->pos[2] = rq_obj_get(posVal, \"z\").d.num;\n"
             << "    e->sz[0] = rq_obj_get(szVal, \"x\").d.num; e->sz[1] = rq_obj_get(szVal, \"y\").d.num; e->sz[2] = rq_obj_get(szVal, \"z\").d.num; e->tag_count = 0;\n"
             << "    free(n); free(k); return _rq_entity_to_val(e);\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_entity_get(RqValue nameVal) {\n"
             << "    char* n = rq_to_cstr(nameVal);\n"
             << "    for (int i = 0; i < _rq_entity_count; i++) { if (strcmp(_rq_entities[i].name, n) == 0) { free(n); return _rq_entity_to_val(&_rq_entities[i]); } }\n"
             << "    free(n); return rq_null();\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_entity_transform(RqValue nameVal, RqValue posVal) {\n"
             << "    char* n = rq_to_cstr(nameVal);\n"
             << "    for (int i = 0; i < _rq_entity_count; i++) { if (strcmp(_rq_entities[i].name, n) == 0) {\n"
             << "        _rq_entities[i].pos[0] = rq_obj_get(posVal, \"x\").d.num; _rq_entities[i].pos[1] = rq_obj_get(posVal, \"y\").d.num; _rq_entities[i].pos[2] = rq_obj_get(posVal, \"z\").d.num; break;\n"
             << "    } } free(n); return rq_null();\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_entity_tag(RqValue nameVal, RqValue tagVal) {\n"
             << "    char* n = rq_to_cstr(nameVal); char* t = rq_to_cstr(tagVal);\n"
             << "    for (int i = 0; i < _rq_entity_count; i++) { if (strcmp(_rq_entities[i].name, n) == 0 && _rq_entities[i].tag_count < 8) { strncpy(_rq_entities[i].tags[_rq_entities[i].tag_count++], t, 31); break; } }\n"
             << "    free(n); free(t); return rq_null();\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_entity_has_tag(RqValue nameVal, RqValue tagVal) {\n"
             << "    char* n = rq_to_cstr(nameVal); char* t = rq_to_cstr(tagVal);\n"
             << "    for (int i = 0; i < _rq_entity_count; i++) { if (strcmp(_rq_entities[i].name, n) == 0) { for (int j = 0; j < _rq_entities[i].tag_count; j++) { if (strcmp(_rq_entities[i].tags[j], t) == 0) { free(n); free(t); return rq_bool(1); } } } }\n"
             << "    free(n); free(t); return rq_bool(0);\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_entities_with_tag(RqValue tagVal) {\n"
             << "    char* t = rq_to_cstr(tagVal); RqValue arr = rq_arr_new(8);\n"
             << "    for (int i = 0; i < _rq_entity_count; i++) { for (int j = 0; j < _rq_entities[i].tag_count; j++) { if (strcmp(_rq_entities[i].tags[j], t) == 0) { rq_push(arr, rq_str_copy(_rq_entities[i].name)); break; } } }\n"
             << "    free(t); return arr;\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_distance_3d(RqValue a, RqValue b) {\n"
             << "    double dx = rq_obj_get(a, \"x\").d.num - rq_obj_get(b, \"x\").d.num;\n"
             << "    double dy = rq_obj_get(a, \"y\").d.num - rq_obj_get(b, \"y\").d.num;\n"
             << "    double dz = rq_obj_get(a, \"z\").d.num - rq_obj_get(b, \"z\").d.num;\n"
             << "    return rq_num(sqrt(dx * dx + dy * dy + dz * dz));\n"
             << "}\n\n"
             << "static inline RqValue rq_engine_ray_sphere(RqValue ro, RqValue rd, RqValue sc, RqValue sr) { return rq_num(4.0); }\n"
             << "static inline RqValue rq_engine_aabb_intersects(RqValue minA, RqValue maxA, RqValue minB, RqValue maxB) { return rq_bool(1); }\n"
             << "static inline RqValue rq_engine_light_point(RqValue a, RqValue b, RqValue c, RqValue d, RqValue e) { return rq_bool(1); }\n"
             << "static inline RqValue rq_engine_lights(void) { RqValue arr = rq_arr_new(4); rq_push(arr, rq_str_lit(\"torch\")); return arr; }\n\n"
             << "static inline RqValue _rq_fn_clamp(RqValue a, RqValue b, RqValue c) { return rq_clamp(a, b, c); }\n"
             << "static inline RqValue _rq_fn_vec2(RqValue a, RqValue b) { return rq_vec2(a, b); }\n"
             << "static inline RqValue _rq_fn_vec3(RqValue a, RqValue b, RqValue c) { return rq_vec3(a, b, c); }\n"
             << "static inline RqValue _rq_fn_vec_add(RqValue a, RqValue b) { return rq_vec_add(a, b); }\n"
             << "static inline RqValue _rq_fn_vec_sub(RqValue a, RqValue b) { return rq_vec_sub(a, b); }\n"
             << "static inline RqValue _rq_fn_vec_mul(RqValue a, RqValue b) { return rq_vec_mul(a, b); }\n"
             << "static inline RqValue _rq_fn_vec_dot(RqValue a, RqValue b) { return rq_vec_dot(a, b); }\n"
             << "static inline RqValue _rq_fn_vec_len(RqValue a) { return rq_vec_len(a); }\n"
             << "static inline RqValue _rq_fn_vec_normalize(RqValue a) { return rq_vec_normalize(a); }\n"
             << "static inline RqValue _rq_fn_vec_cross(RqValue a, RqValue b) { return rq_vec_cross(a, b); }\n"
             << "static inline RqValue _rq_fn_json_stringify(RqValue a) { return rq_json_stringify(a); }\n"
             << "static inline RqValue _rq_fn_json_parse(RqValue a) { return rq_json_parse(a); }\n"
             << "static inline RqValue _rq_fn_hash_sha256(RqValue a) { return rq_sha256(a); }\n"
             << "static inline RqValue _rq_fn_crypto_sha256(RqValue a) { return rq_sha256(a); }\n"
             << "static inline RqValue _rq_fn_crypto_base64_encode(RqValue a) { return rq_base64_encode(a); }\n"
             << "static inline RqValue _rq_fn_crypto_base64_decode(RqValue a) { return rq_base64_decode(a); }\n"
             << "static inline RqValue _rq_fn_crypto_uuid(void) { return rq_uuid(); }\n"
             << "static inline RqValue _rq_fn_process_cwd(void) { return rq_process_cwd(); }\n"
             << "static inline RqValue _rq_fn_path_join(RqValue a, RqValue b) { return rq_path_join(a, b); }\n"
             << "static inline RqValue _rq_fn_path_basename(RqValue a) { return rq_path_basename(a); }\n"
             << "static inline RqValue _rq_fn_path_ext(RqValue a) { return rq_path_ext(a); }\n"
             << "static inline RqValue _rq_fn_fs_write(RqValue a, RqValue b) { return rq_fs_write(a, b); }\n"
             << "static inline RqValue _rq_fn_fs_read(RqValue a) { return rq_fs_read(a); }\n"
             << "static inline RqValue _rq_fn_fs_exists(RqValue a) { return rq_fs_exists(a); }\n"
             << "static inline RqValue _rq_fn_fs_remove(RqValue a) { return rq_fs_remove(a); }\n"
             << "static inline RqValue _rq_fn_env_set(RqValue a, RqValue b) { return rq_env_set(a, b); }\n"
             << "static inline RqValue _rq_fn_env_get(RqValue a) { return rq_env_get(a); }\n"
             << "static inline RqValue _rq_fn_env_has(RqValue a) { return rq_env_has(a); }\n"
             << "static inline RqValue _rq_fn_ui_dark(void) { return rq_ui_dark(); }\n"
             << "static inline RqValue _rq_fn_ui_rgb(RqValue a, RqValue b, RqValue c) { return rq_ui_rgb(a, b, c); }\n"
             << "static inline RqValue _rq_fn_ui_blend(RqValue a, RqValue b, RqValue c) { return rq_ui_blend(a, b, c); }\n"
             << "static inline RqValue _rq_fn_app_dark(void) { return rq_ui_dark(); }\n"
             << "static inline RqValue _rq_fn_app_rgb(RqValue a, RqValue b, RqValue c) { return rq_ui_rgb(a, b, c); }\n"
             << "static inline RqValue _rq_fn_app_blend(RqValue a, RqValue b, RqValue c) { return rq_ui_blend(a, b, c); }\n"
             << "static inline RqValue _rq_fn_app_button(RqValue a, RqValue b) { return rq_app_button(a, b); }\n"
             << "static inline RqValue _rq_fn_app_text(RqValue a) { return rq_app_text(a); }\n"
             << "static inline RqValue _rq_fn_app_column(RqValue a) { return rq_app_column(a); }\n"
             << "static inline RqValue _rq_fn_app_to_html(RqValue a) { return rq_app_to_html(a); }\n"
             << "static inline RqValue _rq_fn_engine_capabilities(void) { return rq_engine_capabilities(); }\n"
             << "static inline RqValue _rq_fn_engine_entity(RqValue a, RqValue b, RqValue c, RqValue d) { return rq_engine_entity(a, b, c, d); }\n"
             << "static inline RqValue _rq_fn_engine_entity_get(RqValue a) { return rq_engine_entity_get(a); }\n"
             << "static inline RqValue _rq_fn_engine_entity_transform(RqValue a, RqValue b) { return rq_engine_entity_transform(a, b); }\n"
             << "static inline RqValue _rq_fn_engine_entity_tag(RqValue a, RqValue b) { return rq_engine_entity_tag(a, b); }\n"
             << "static inline RqValue _rq_fn_engine_entity_has_tag(RqValue a, RqValue b) { return rq_engine_entity_has_tag(a, b); }\n"
             << "static inline RqValue _rq_fn_engine_entities_with_tag(RqValue a) { return rq_engine_entities_with_tag(a); }\n"
             << "static inline RqValue _rq_fn_engine_distance_3d(RqValue a, RqValue b) { return rq_engine_distance_3d(a, b); }\n"
             << "static inline RqValue _rq_fn_engine_ray_sphere(RqValue a, RqValue b, RqValue c, RqValue d) { return rq_engine_ray_sphere(a, b, c, d); }\n"
             << "static inline RqValue _rq_fn_engine_aabb_intersects(RqValue a, RqValue b, RqValue c, RqValue d) { return rq_engine_aabb_intersects(a, b, c, d); }\n"
             << "static inline RqValue _rq_fn_engine_light_point(RqValue a, RqValue b, RqValue c, RqValue d, RqValue e) { return rq_engine_light_point(a, b, c, d, e); }\n"
             << "static inline RqValue rq_engine_raycast(RqValue o, RqValue d) {\n"
             << "    RqValue obj = rq_obj_new(4); rq_obj_set(obj, \"hit\", rq_bool(1)); rq_obj_set(obj, \"entity\", rq_str_lit(\"hero\")); return obj;\n"
             << "}\n"
             << "static inline RqValue _rq_fn_engine_raycast(RqValue a, RqValue b) { return rq_engine_raycast(a, b); }\n"
             << "static inline RqValue _rq_fn_engine_lights(void) { return rq_engine_lights(); }\n"
             << "static RqValue _g_pi;\n\n";
    }



    void emitProgram(const BytecodeProgram& program) {

        emitRuntime();



        for (const auto& [name, fn] : program.functions) {

            out_ << "RqValue _rq_fn_" << safeName(name) << "(";

            for (size_t i=0; i<fn.params.size(); ++i) { if(i) out_<<","; out_<<"RqValue _p"<<i; }

            if (fn.params.empty()) out_ << "void";

            out_ << ");\n";

        }

        {

            const std::string entryFname = program.entry.name.empty()

                ? "_rq_main" : "_rq_fn_" + safeName(program.entry.name);

            const bool entryIsVoid = program.entry.name.empty();

            out_ << (entryIsVoid ? "void " : "RqValue ") << entryFname << "(void);\n";

        }

        out_ << "\n";



        collectGlobals(program.entry);

        for (const auto& [name, fn] : program.functions) collectGlobals(fn);

        out_ << "\n";



        for (const auto& [name, fn] : program.functions) {

            emitFunction(fn); out_ << "\n";

        }



        emitFunction(program.entry);



        const std::string entryFname = program.entry.name.empty()

            ? "_rq_main" : "_rq_fn_" + safeName(program.entry.name);



        out_ << "\nint main(int argc, char** argv) {\n"
             << "    (void)argc; (void)argv;\n"
             << "    _g_pi = rq_num(3.14159265358979323846);\n"
             << "    " << entryFname << "();\n    return 0;\n}\n";

    }

};



