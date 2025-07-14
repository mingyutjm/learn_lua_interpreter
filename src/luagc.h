#ifndef luagc_h
#define luagc_h

#include "luastate.h"

// GCState
#define GCSpause 0
#define GCSpropagate 1
#define GCSatomic 2
#define GCSinsideatomic 3
#define GCSsweepallgc 4
#define GCSsweepend 5

// Color
#define WHITE0BIT 0
#define WHITE1BIT 1
#define BLACKBIT 2

// Bit operation
#define bitmask(b) (1 << b)                          // b位是1，其他0
#define bit2mask(b1, b2) (bitmask(b1) | bitmask(b2)) // b1 和 b2 位是1，其他0
#define resetbits(x, m) ((x) &= cast(lu_byte, ~(m))) // 把mask中标1的位 置0
#define setbits(x, m) ((x) |= (m))                   // 把mask中标1的位 置1
#define testbits(x, m) ((x) & (m))                   // 存在相同位为1则结果不为0
#define resetbit(x, b) resetbits(x, bitmask(b))      // 把第b位 置0
#define l_setbit(x, b) setbits(x, bitmask(b))        // 把第b位 置1
#define testbit(x, b) testbits(x, bitmask(b))        // 第b位是否是1

//
#define WHITEBITS bit2mask(WHITE0BIT, WHITE1BIT)    // 白色mask, 是哪几位
#define luaC_white(g) (g->currentwhite & WHITEBITS) // 获取当前白色
#define otherwhite(g) (g->currentwhite ^ WHITEBITS) // 获取其他白色

#define iswhite(o) testbits((o)->marked, WHITEBITS)
#define isgray(o) (!testbits((o)->marked, bitmask(BLACKBIT) | WHITEBITS)) // 不是白色也不是黑色，就是全0
#define isblack(o) testbit((o)->marked, bitmask(BLACKBIT))
#define isdeadm(ow, m) (((m ^ WHITEBITS) & (ow)) == 0)   // mark是否和ow的颜色一样(只看白色位)
#define isdead(g, o) isdeadm(otherwhite(g), (o)->marked) // g(global_state), o(GCObject)
#define changewhite(o) ((o)->marked ^= WHITEBITS)        // 切换到otherwhite(如果本来是白色的话)

// o必须是GCObject，或者第一个成员必须是GCObject
#define obj2gco(o) (&cast(union GCUnion *, o)->gc)
// th (thread), o必须是lua_State或者第一个成员必须是lua_State
#define gco2th(o) check_exp((o)->tt_ == LUA_TTHREAD, &cast(union GCUnion *, o)->th)
// o必须是一个TString
#define gco2ts(o) check_exp((o)->tt_ == LUA_SHRSTR || (o)->tt_ == LUA_LNGSTR, &cast(union GCUnion *, o)->ts)
// o必须是GCObject，或者第一个成员必须是GCObject
#define gcvalue(o) ((o)->value_.gc)

#define iscollectable(o)        \
    ((o)->tt_ == LUA_TTHREAD || \
     (o)->tt_ == LUA_SHRSTR ||  \
     (o)->tt_ == LUA_LNGSTR ||  \
     (o)->tt_ == LUA_TTABLE ||  \
     (o)->tt_ == LUA_TLCL ||    \
     (o)->tt_ == LUA_TCCL)

//
#define markobject(L, o)                 \
    if (iswhite(o))                      \
    {                                    \
        reallymarkobject(L, obj2gco(o)); \
    }

#define markvalue(L, o)                          \
    if (iscollectable(o) && iswhite(gcvalue(o))) \
    {                                            \
        reallymarkobject(L, gcvalue(o));         \
    }

// 头插链接
#define linkgclist(gco, prev) \
    {                         \
        (gco)->gclist = prev; \
        prev = obj2gco(gco);  \
    }

//
#define luaC_condgc(prev, L, post) \
    if (G(L)->GCdebt > 0)          \
    {                              \
        prev;                      \
        luaC_step(L);              \
        post;                      \
    }
#define luaC_checkgc(L) luaC_condgc((void)0, L, (void)0)

//
GCObject *luaC_newobj(struct lua_State *L, int tt_, size_t size);
void luaC_step(struct lua_State *L);
void luaC_fix(struct lua_State *L, struct GCObject *o); // GCObject can not collect
void reallymarkobject(struct lua_State *L, struct GCObject *gco);
void luaC_freeallobjects(struct lua_State *L);

#endif