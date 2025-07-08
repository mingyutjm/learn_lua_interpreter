#ifndef luastate_h
#define luastate_h

#include "luaobject.h"

#define LUA_EXTRASPACE sizeof(void *)
#define G(L) ((L)->l_G)

typedef TValue *StkId;

struct CallInfo
{
    StkId func;                // 被调用函数在栈中的位置
    StkId top;                 // 被调用函数的栈顶位置
    int nresult;               // 有多少个返回值
    int callstatus;            // 调用状态
    struct CallInfo *next;     // 下一个调用
    struct CallInfo *previous; // 上一个调用
};

typedef struct lua_State
{
    StkId stack;                  // 栈
    StkId stack_last;             // 从这里开始，栈不能被使用
    StkId top;                    // 栈顶 + 1，调用函数时动态改变
    int stack_size;               // 栈的整体大小
    struct lua_longjmp *errorjmp; // 保护模式中，要用到的结构，当异常抛出时，跳出逻辑
    int status;                   // lua_State的状态
    struct lua_State *next;       // 下一个lua_State，通常创建协程时会产生
    struct lua_State *previous;
    struct CallInfo base_ci;  // 和lua_State生命周期一致的函数调用信息
    struct CallInfo *ci;      // 当前运作的CallInfo
    struct global_State *l_G; // global_State指针
    ptrdiff_t errorfunc;      // 错误函数位于栈的哪个位置
    int ncalls;               // 进行多少次函数调用
} lua_State;

typedef struct global_State
{
    // lua_State其实是lua thread，某种程度上来说，它也是协程
    struct lua_State *mainthread;

    // 一个可以自定义的内存分配函数
    lua_Alloc frealloc;

    // 当我们自定义内存分配器时，可能要用到这个结构，但是我们用官方默认的版本,因此它始终是NULL
    void *ud;

    // 当调用LUA_THROW接口时，如果当前不处于保护模式，那么会直接调用panic函数
    // panic函数通常是输出一些关键日志
    lua_CFunction panic;

} global_State;

struct lua_State *lua_newstate(lua_Alloc alloc, void *ud);
void lua_close(struct lua_State *L);

void setivalue(StkId target, int integer);
void setfvalue(StkId target, lua_CFunction f);
void setfltvalue(StkId target, float number);
void setbvalue(StkId target, bool b);
void setnilvalue(StkId target);
void setpvalue(StkId target, void *p);
void setobj(StkId target, StkId value);

void increase_top(struct lua_State *L);
void lua_pushcfunction(struct lua_State *L, lua_CFunction f);
void lua_pushinteger(struct lua_State *L, int integer);
void lua_pushnumber(struct lua_State *L, float number);
void lua_pushboolean(struct lua_State *L, bool b);
void lua_pushnil(struct lua_State *L);
void lua_pushlightuserdata(struct lua_State *L, void *p);

lua_Integer lua_tointegerx(struct lua_State *L, int idx, int *isnum);
lua_Number lua_tonumberx(struct lua_State *L, int idx, int *isnum);
bool lua_toboolean(struct lua_State *L, int idx);
int lua_isnil(struct lua_State *L, int idx);

void lua_settop(struct lua_State *L, int idx);
int lua_gettop(struct lua_State *L);
void lua_pop(struct lua_State *L);

#endif
