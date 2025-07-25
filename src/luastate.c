#include "luastate.h"
#include "luamem.h"
#include "luagc.h"
#include "luastring.h"

// 先把L转换成 byte*, 然后减去L在LX中的偏移量，得到LX的指针，就相当于LG指针
#define fromstate(L) (cast(LG *, cast(lu_byte *, (L)) - offsetof(LX, l)))

typedef struct LX
{
    lu_byte extra_[LUA_EXTRASPACE];
    lua_State l;
} LX;

typedef struct LG
{
    LX l;
    global_State g;
} LG;

static void stack_init(struct lua_State *L)
{
    L->stack = (StkId)luaM_realloc(L, NULL, 0, LUA_STACKSIZE * sizeof(TValue));
    L->stack_size = LUA_STACKSIZE;
    L->stack_last = L->stack + LUA_STACKSIZE - LUA_EXTRASTACK;
    L->next = NULL;
    L->previous = NULL;
    L->status = LUA_OK;
    L->errorjmp = NULL;
    L->top = L->stack;
    L->errorfunc = 0;

    for (int i = 0; i < L->stack_size; i++)
    {
        setnilvalue(L->stack + i);
    }
    L->top++;

    L->ci = &L->base_ci;
    L->ci->func = L->stack;
    L->ci->top = L->stack + LUA_MINSTACK;
    L->ci->previous = NULL;
    L->ci->next = NULL;
}

static void free_stack(struct lua_State *L)
{
    global_State *g = G(L);
    luaM_free(L, L->stack, sizeof(TValue));
    L->stack = L->stack_last = L->top = NULL;
    L->stack_size = 0;
}

#define addbuff(b, t, p)         \
    {                            \
        memcpy(b, t, sizeof(t)); \
        p += sizeof(t);          \
    }

static u32 makeseed(struct lua_State *L)
{
    char buff[4 * sizeof(size_t)];
    u32 h = time(NULL);
    int p = 0;
    addbuff(buff, L, p);
    addbuff(buff, &h, p);
    addbuff(buff, luaO_nilobject, p);
    addbuff(buff, &lua_newstate, p);
    return luaS_hash(L, buff, p, h);
}

struct lua_State *lua_newstate(lua_Alloc alloc, void *ud)
{
    struct global_State *g;
    struct lua_State *L;

    struct LG *lg = (struct LG *)(*alloc)(ud, NULL, LUA_TTHREAD, sizeof(struct LG));
    if (!lg)
        return NULL;

    g = &(lg->g);
    g->ud = ud;
    g->frealloc = alloc;
    g->panic = NULL;

    L = &(lg->l.l);
    L->tt_ = LUA_TTHREAD;
    L->nci = 0;
    G(L) = g; // L->l_G = g;
    g->mainthread = L;

    // string seed
    g->seed = makeseed(L);

    // gc init
    g->gcstate = GCSpause;
    g->currentwhite = bitmask(WHITE0BIT);
    g->totalbytes = sizeof(LG);
    g->allgc = NULL;
    g->gray = NULL;
    g->grayagain = NULL;
    g->sweepgc = NULL;
    g->fixgc = NULL;
    g->GCdebt = 0;
    g->GCmemtrav = 0;
    g->GCestimate = 0;
    g->GCstepmul = LUA_GCSTEPMUL;
    L->marked = luaC_white(g);
    L->gclist = NULL;

    stack_init(L);

    luaS_init(L);

    return L;
}

void lua_close(struct lua_State *L)
{
    struct global_State *g = G(L);
    struct lua_State *L1 = g->mainthread; // only mainthread can be close

    // 释放CallInfo
    struct CallInfo *ci = &L1->base_ci;
    // 不释放base_ci, 因为base_ci不是alloc出来的
    while (ci->next)
    {
        struct CallInfo *next = ci->next->next;
        struct CallInfo *free_ci = ci->next;

        luaM_free(L, free_ci, sizeof(struct CallInfo));
        ci = next;
    }

    // 释放栈空间
    free_stack(L1);

    // 释放全局状态
    // (*g->frealloc)(g->ud, fromstate(L1), sizeof(LG), 0);
    luaM_free(L, fromstate(L1), sizeof(LG));
}

void setivalue(StkId target, int integer)
{
    target->value_.i = integer;
    target->tt_ = LUA_NUMINT;
}

void setfvalue(StkId target, lua_CFunction f)
{
    target->value_.f = f;
    target->tt_ = LUA_TLCF;
}

void setfltvalue(StkId target, float number)
{
    target->value_.n = number;
    target->tt_ = LUA_NUMFLT;
}

void setbvalue(StkId target, bool b)
{
    target->value_.b = b ? 1 : 0;
    target->tt_ = LUA_TBOOLEAN;
}

void setnilvalue(StkId target)
{
    target->tt_ = LUA_TNIL;
}

void setpvalue(StkId target, void *p)
{
    target->value_.p = p;
    target->tt_ = LUA_TLIGHTUSERDATA;
}

void setgco(StkId target, struct GCObject *gco)
{
    target->value_.gc = gco;
    target->tt_ = gco->tt_;
}

void setobj(StkId target, StkId value)
{
    target->value_ = value->value_;
    target->tt_ = value->tt_;
}

void increase_top(struct lua_State *L)
{
    L->top++;
    assert(L->top <= L->stack_last);
}

void lua_pushcfunction(struct lua_State *L, lua_CFunction f)
{
    setfvalue(L->top, f);
    increase_top(L);
}

void lua_pushinteger(struct lua_State *L, int integer)
{
    setivalue(L->top, integer);
    increase_top(L);
}

void lua_pushnumber(struct lua_State *L, float number)
{
    setfltvalue(L->top, number);
    increase_top(L);
}

void lua_pushboolean(struct lua_State *L, bool b)
{
    setbvalue(L->top, b);
    increase_top(L);
}

void lua_pushnil(struct lua_State *L)
{
    setnilvalue(L->top);
    increase_top(L);
}

void lua_pushlightuserdata(struct lua_State *L, void *p)
{
    setpvalue(L->top, p);
    increase_top(L);
}

void lua_pushstring(struct lua_State *L, const char *str)
{
    u32 l = strlen(str);
    struct TString *ts = luaS_newlstr(L, str, l);
    struct GCObject *gco = obj2gco(ts);
    setgco(L->top, gco);
    increase_top(L);
}

TValue *index2addr(struct lua_State *L, int idx)
{
    if (idx >= 0)
    {
        assert(L->ci->func + idx < L->ci->top);
        return L->ci->func + idx;
    }
    else
    {
        assert(L->top + idx > L->ci->func);
        return L->top + idx;
    }
}

lua_Integer lua_tointegerx(struct lua_State *L, int idx, int *isnum)
{
    lua_Integer ret = 0;
    TValue *addr = index2addr(L, idx);
    if (addr->tt_ == LUA_NUMINT)
    {
        ret = addr->value_.i;
        *isnum = 1;
    }
    else
    {
        *isnum = 0;
        LUA_ERROR(L, "can not convert to integer!\n");
    }

    return ret;
}

lua_Number lua_tonumberx(struct lua_State *L, int idx, int *isnum)
{
    lua_Number ret = 0.0f;
    TValue *addr = index2addr(L, idx);
    if (addr->tt_ == LUA_NUMFLT)
    {
        *isnum = 1;
        ret = addr->value_.n;
    }
    else
    {
        LUA_ERROR(L, "can not convert to number!");
        *isnum = 0;
    }
    return ret;
}

bool lua_toboolean(struct lua_State *L, int idx)
{
    TValue *addr = index2addr(L, idx);
    return !(addr->tt_ == LUA_TNIL || addr->value_.b == 0);
}

char *lua_tostring(struct lua_State *L, int idx)
{
    TValue *addr = index2addr(L, idx);
    if (novariant(addr) != LUA_TSTRING)
        return NULL;
    struct TString *ts = gco2ts(addr->value_.gc);
    return getstr(ts);
}

int lua_isnil(struct lua_State *L, int idx)
{
    TValue *addr = index2addr(L, idx);
    return addr->tt_ == LUA_TNIL;
}

// 栈的大小
int lua_gettop(struct lua_State *L)
{
    return cast(int, L->top - (L->ci->func + 1));
}

void lua_settop(struct lua_State *L, int idx)
{
    StkId func = L->ci->func;
    if (idx >= 0)
    {
        assert(idx <= L->stack_last - (func + 1));
        while (L->top < (func + 1) + idx)
        {
            setnilvalue(L->top++);
        }
        L->top = func + 1 + idx;
    }
    else
    {
        assert(L->top + idx > func);
        L->top = L->top + idx;
    }
}

void lua_pop(struct lua_State *L)
{
    lua_settop(L, -1);
}
