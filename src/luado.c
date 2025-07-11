#include "luado.h"
#include "luamem.h"

// c需要是一个lua_longjmp
#define LUA_TRY(L, c, a)      \
    if (_setjmp((c)->b) == 0) \
    {                         \
        a                     \
    }

// #ifdef _WINDOWS_PLATFORM_
// #define LUA_THROW(c) longjmp((c)->b, 1)
// #else
// #define LUA_THROW(c) _longjmp((c)->b, 1)
// #endif
#define LUA_THROW(c) longjmp((c)->b, 1)

struct lua_longjmp
{
    struct lua_longjmp *previous;
    jmp_buf b;
    int status;
};

void seterrobj(struct lua_State *L, int error)
{
    lua_pushinteger(L, error);
}

void luaD_checkstack(struct lua_State *L, int need)
{
    if (L->top + need > L->stack_last)
    {
        luaD_growstack(L, need);
    }
}

void luaD_growstack(struct lua_State *L, int size)
{
    if (L->stack_size > LUA_MAXSTACK)
    {
        luaD_throw(L, LUA_ERRERR);
    }

    int stack_size = L->stack_size * 2;
    // 已经占用的 + 需要的 + 占位 （TODO: 有点不对啊，说不定会变小）
    int need_size = cast(int, L->top - L->stack) + size + LUA_EXTRASTACK;
    if (stack_size < need_size)
    {
        stack_size = need_size;
    }

    if (stack_size > LUA_MAXSTACK)
    {
        stack_size = LUA_MAXSTACK + LUA_ERRORSTACK;
        LUA_ERROR(L, "stack overflow");
    }

    TValue *old_stack = L->stack;
    L->stack = luaM_realloc(L, L->stack, L->stack_size, stack_size * sizeof(TValue));
    L->stack_size = stack_size;
    L->stack_last = L->stack + stack_size - LUA_EXTRASTACK;
    // TODO: 可以放到分配内存前做
    int top_diff = cast(int, L->top - old_stack);
    L->top = restorestack(L, top_diff);

    // 指针重建
    struct CallInfo *ci;
    ci = &L->base_ci;
    while (ci)
    {
        int func_diff = cast(int, ci->func - old_stack);
        int top_diff = cast(int, ci->top - old_stack);
        ci->func = restorestack(L, func_diff);
        ci->top = restorestack(L, top_diff);

        ci = ci->next;
    }
}

void luaD_throw(struct lua_State *L, int error)
{
    struct global_State *g = G(L);
    if (L->errorjmp)
    {
        L->errorjmp->status = error;
        LUA_THROW(L->errorjmp);
    }
    else
    {
        if (g->panic)
        {
            (*g->panic)(L);
        }
        abort();
    }
}

int luaD_rawrunprotected(struct lua_State *L, Pfunc f, void *ud)
{
    int old_ncalls = L->ncalls;
    struct lua_longjmp lj;
    lj.previous = L->errorjmp;
    lj.status = LUA_OK;
    L->errorjmp = &lj;

    LUA_TRY(
        L,
        L->errorjmp,
        (*f)(L, ud);)

    L->errorjmp = lj.previous;
    L->ncalls = old_ncalls;
    return lj.status;
}

// 构造一个 CallInfo
static struct CallInfo *next_ci(struct lua_State *L, StkId func, int nresult)
{
    struct global_State *g = G(L);
    struct CallInfo *ci;
    ci = luaM_realloc(L, NULL, 0, sizeof(struct CallInfo));
    ci->next = NULL;
    ci->previous = L->ci;
    L->ci->next = ci;
    ci->nresult = nresult;
    ci->callstatus = LUA_OK;
    ci->func = func;
    // NOTE: 注意这里, ci->top 是当前L->top + LUA_MINSTACK
    ci->top = L->top + LUA_MINSTACK;
    L->ci = ci;
    return ci;
}

int luaD_call(struct lua_State *L, StkId func, int nresult)
{
    if (++L->ncalls > LUA_MAXCALLS)
    {
        luaD_throw(L, 0);
    }

    if (luaD_precall(L, func, nresult) == 0)
    {
        // TODO luaV_execute(L);
    }

    L->ncalls--;
    return LUA_OK;
}

static void reset_unuse_stack(struct lua_State *L, ptrdiff_t old_top)
{
    struct global_State *g = G(L);
    StkId top = restorestack(L, old_top);
    for (; top < L->top; top++)
    {
        top->value_.p = NULL;
        top->tt_ = LUA_TNIL;
    }
}

int luaD_pcall(struct lua_State *L, Pfunc f, void *ud, ptrdiff_t oldtop, ptrdiff_t ef)
{
    int status;
    struct CallInfo *old_ci = L->ci;
    ptrdiff_t old_errorfunc = L->errorfunc;

    status = luaD_rawrunprotected(L, f, ud);
    if (status != LUA_OK)
    {
        // because we have not implement gc, so we should free ci manually
        // struct global_State *g = G(L);
        // struct CallInfo *free_ci = L->ci;
        // while (free_ci)
        // {
        //     if (free_ci == old_ci)
        //     {
        //         free_ci = free_ci->next;
        //         continue;
        //     }

        //     struct CallInfo *previous = free_ci->previous;
        //     previous->next = NULL;

        //     struct CallInfo *next = free_ci->next;
        //     (*g->frealloc)(g->ud, free_ci, sizeof(struct CallInfo), 0);
        //     free_ci = next;
        // }

        reset_unuse_stack(L, oldtop);
        L->ci = old_ci;
        L->top = restorestack(L, oldtop);
        seterrobj(L, status);
    }

    L->errorfunc = old_errorfunc;
    return status;
}

// prepare for function call.
// if we call a c function, just directly call it
// if we call a lua function, prepare for call it
int luaD_precall(struct lua_State *L, StkId func, int nresult)
{
    switch (func->tt_)
    {
    case LUA_TLCF:
    {
        lua_CFunction f = func->value_.f;

        ptrdiff_t func_diff = func - L->stack;
        luaD_checkstack(L, LUA_MINSTACK); // 检查lua_State的空间是否充足，如果不充足，则需要扩展
        func = restorestack(L, func_diff);

        next_ci(L, func, nresult);
        int n = (*f)(L); // 对func指向的函数进行调用，并获取实际返回值的数量
        assert(L->ci->func + n <= L->ci->top);
        luaD_poscall(L, L->top - n, n); // 处理返回值
        return 1;
    }
    break;
    default:
        break;
    }

    return 0;
}

// first_result: 第一个返回值的位置，nresult: 返回了几个值（可能并不是预定的数量）
int luaD_poscall(struct lua_State *L, StkId first_result, int nresult)
{
    StkId func = L->ci->func;
    int nwant = L->ci->nresult;

    switch (nwant)
    {
    // 期待0个
    case 0:
    {
        // 相当于把func退栈
        L->top = L->ci->func;
        break;
    }
    // 期待1个返回值
    case 1:
    {
        // 实际上获得了0个
        if (nresult == 0)
        {
            // 相当于返回了一个 nil
            first_result->value_.p = NULL;
            first_result->tt_ = LUA_TNIL;
        }
        // 把返回值写到func的位置
        setobj(func, first_result);
        // 清理
        first_result->value_.p = NULL;
        first_result->tt_ = LUA_TNIL;

        // 移动top到 func + 1
        L->top = func + nwant;
        break;
    }
    // 实际有多少个返回值，就返回多少个，不会丢弃
    case LUA_MULRET:
    {
        // 实际上的返回值个数
        int nres = cast(int, L->top - first_result);
        // 从func开始，用返回值覆盖
        for (int i = 0; i < nres; i++)
        {
            StkId current = first_result + i;
            setobj(func + i, current);
            current->value_.p = NULL;
            current->tt_ = LUA_TNIL;
        }
        L->top = func + nres;
        break;
    }
    // 其他，多返回值的情况
    default:
    {
        // 返回的比期望的少
        if (nwant > nresult)
        {
            for (int i = 0; i < nwant; i++)
            {
                if (i < nresult)
                {
                    StkId current = first_result + i;
                    setobj(func + i, current);
                    current->value_.p = NULL;
                    current->tt_ = LUA_TNIL;
                }
                else
                {
                    StkId stack = func + i;
                    stack->tt_ = LUA_TNIL;
                }
            }
            L->top = func + nwant;
        }
        // 返回的和期望的相等或更多，返回多的，但是多出来的直接置空
        else
        {
            for (int i = 0; i < nresult; i++)
            {
                if (i < nwant)
                {
                    StkId current = first_result + i;
                    setobj(func + i, current);
                    current->value_.p = NULL;
                    current->tt_ = LUA_TNIL;
                }
                else
                // 超出个数的，置空
                {
                    StkId stack = func + i;
                    stack->value_.p = NULL;
                    stack->tt_ = LUA_TNIL;
                }
            }
            L->top = func + nresult;
        }
        break;
    }
    }

    // 函数退出
    struct CallInfo *ci = L->ci;
    L->ci = ci->previous;
    L->ci->next = NULL;

    // because we have not implement gc, so we should free ci manually
    // 释放 CallInfo 内存
    // struct global_State *g = G(L);
    // (*g->frealloc)(g->ud, ci, sizeof(struct CallInfo), 0);

    return LUA_OK;
}
