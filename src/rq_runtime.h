#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

typedef enum { RQ_NULL = 0, RQ_NUM = 1, RQ_STR = 2, RQ_BOOL = 3 } RqType;
typedef struct RqValue { RqType type; union { double num; char* str; int b; } d; } RqValue;

static inline RqValue rq_null(void)       { RqValue v; v.type=RQ_NULL; v.d.num=0; return v; }
static inline RqValue rq_num(double n)    { RqValue v; v.type=RQ_NUM; v.d.num=n; return v; }
static inline RqValue rq_bool(int b)      { RqValue v; v.type=RQ_BOOL; v.d.b=(b!=0); return v; }
static inline RqValue rq_str_copy(const char* src) {
    RqValue v; v.type=RQ_STR;
    if (!src) { v.d.str=(char*)malloc(1); v.d.str[0]=0; return v; }
    size_t l=strlen(src); v.d.str=(char*)malloc(l+1); memcpy(v.d.str,src,l+1); return v;
}
static inline int rq_truthy(RqValue v) {
    switch(v.type){case RQ_NULL:return 0;case RQ_BOOL:return v.d.b;
    case RQ_NUM:return v.d.num!=0.0;case RQ_STR:return v.d.str&&v.d.str[0]!=0;default:return 0;}
}
static inline char* rq_to_cstr(RqValue v) {
    char buf[64];
    switch(v.type){
    case RQ_NULL: return strdup("null");
    case RQ_BOOL: return strdup(v.d.b?"true":"false");
    case RQ_STR:  return strdup(v.d.str?v.d.str:"");
    case RQ_NUM: {
        double n=v.d.num;
        if(n==(long long)n&&n>=-1e15&&n<=1e15) snprintf(buf,sizeof(buf),"%lld",(long long)n);
        else snprintf(buf,sizeof(buf),"%g",n);
        return strdup(buf);}
    default: return strdup("");}
}
static inline RqValue rq_to_str(RqValue v){char*s=rq_to_cstr(v);RqValue r=rq_str_copy(s);free(s);return r;}

static inline RqValue rq_add(RqValue a,RqValue b){
    if(a.type==RQ_STR||b.type==RQ_STR){
        char*sa=rq_to_cstr(a),*sb=rq_to_cstr(b);
        size_t la=strlen(sa),lb=strlen(sb);
        char*res=(char*)malloc(la+lb+1);memcpy(res,sa,la);memcpy(res+la,sb,lb+1);
        free(sa);free(sb); RqValue r;r.type=RQ_STR;r.d.str=res;return r;}
    return rq_num(a.d.num+b.d.num);}
static inline RqValue rq_sub(RqValue a,RqValue b){return rq_num(a.d.num-b.d.num);}
static inline RqValue rq_mul(RqValue a,RqValue b){return rq_num(a.d.num*b.d.num);}
static inline RqValue rq_div(RqValue a,RqValue b){return rq_num(b.d.num!=0.0?a.d.num/b.d.num:0.0);}
static inline RqValue rq_mod(RqValue a,RqValue b){
    long long ia=(long long)a.d.num,ib=(long long)b.d.num;
    return rq_num(ib!=0?(double)(ia%ib):0.0);}
static inline RqValue rq_neg(RqValue a){return rq_num(-a.d.num);}
static inline RqValue rq_eq(RqValue a,RqValue b){
    if(a.type!=b.type)return rq_bool(0);
    switch(a.type){case RQ_NULL:return rq_bool(1);case RQ_BOOL:return rq_bool(a.d.b==b.d.b);
    case RQ_NUM:return rq_bool(a.d.num==b.d.num);case RQ_STR:return rq_bool(strcmp(a.d.str,b.d.str)==0);
    default:return rq_bool(0);}}
static inline RqValue rq_ne(RqValue a,RqValue b){return rq_bool(!rq_truthy(rq_eq(a,b)));}
static inline RqValue rq_lt(RqValue a,RqValue b){return rq_bool(a.d.num<b.d.num);}
static inline RqValue rq_le(RqValue a,RqValue b){return rq_bool(a.d.num<=b.d.num);}
static inline RqValue rq_gt(RqValue a,RqValue b){return rq_bool(a.d.num>b.d.num);}
static inline RqValue rq_ge(RqValue a,RqValue b){return rq_bool(a.d.num>=b.d.num);}
static inline RqValue rq_not(RqValue a){return rq_bool(!rq_truthy(a));}
static inline RqValue rq_coalesce(RqValue a,RqValue b){return(a.type==RQ_NULL)?b:a;}

static inline RqValue rq_print(RqValue v){char*s=rq_to_cstr(v);printf("%s\n",s);free(s);return rq_null();}
static inline RqValue rq_str_builtin(RqValue v){return rq_to_str(v);}
static inline RqValue rq_num_builtin(RqValue v){
    switch(v.type){case RQ_NUM:return v;case RQ_BOOL:return rq_num(v.d.b?1.0:0.0);
    case RQ_STR:return rq_num(atof(v.d.str));default:return rq_num(0.0);}}
static inline RqValue rq_bool_builtin(RqValue v){return rq_bool(rq_truthy(v));}
static inline RqValue rq_type_builtin(RqValue v){
    switch(v.type){case RQ_NULL:return rq_str_copy("null");case RQ_NUM:return rq_str_copy("number");
    case RQ_STR:return rq_str_copy("string");case RQ_BOOL:return rq_str_copy("bool");
    default:return rq_str_copy("unknown");}}
static inline RqValue rq_sqrt(RqValue v){return rq_num(sqrt(v.d.num));}
static inline RqValue rq_abs(RqValue v){return rq_num(fabs(v.d.num));}
static inline RqValue rq_floor(RqValue v){return rq_num(floor(v.d.num));}
static inline RqValue rq_ceil(RqValue v){return rq_num(ceil(v.d.num));}
static inline RqValue rq_round(RqValue v){return rq_num(round(v.d.num));}
static inline RqValue rq_pow(RqValue a,RqValue b){return rq_num(pow(a.d.num,b.d.num));}
static inline RqValue rq_concat_n(RqValue* parts,int n){
    if(n<=0)return rq_str_copy("");
    size_t total=0;
    char** ptrs=(char**)malloc((size_t)n*sizeof(char*));
    for(int i=0;i<n;++i){ptrs[i]=rq_to_cstr(parts[i]);total+=strlen(ptrs[i]);}
    char* buf=(char*)malloc(total+1);buf[0]=0;
    for(int i=0;i<n;++i){strcat(buf,ptrs[i]);free(ptrs[i]);}
    free(ptrs);
    RqValue r;r.type=RQ_STR;r.d.str=buf;return r;
}
static inline void rq_throw_msg(const char* msg){fprintf(stderr,"error: %s\n",msg?msg:"");exit(1);}

