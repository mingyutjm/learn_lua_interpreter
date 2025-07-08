// #include <setjmp.h>
// #include <stdarg.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <stdbool.h>
// #include <stddef.h>
// #include <assert.h>
// #include <stdint.h>

// typedef int8_t int8;
// typedef int16_t int16;
// typedef int32_t int32;
// typedef int64_t int64;
// typedef uint8_t uint8;
// typedef uint16_t uint16;
// typedef uint32_t uint32;
// typedef uint64_t uint64;

// #define LUA_TNONE (-1)
// #define LUA_TNIL 0
// #define LUA_TNUMBER 1
// #define LUA_TLIGHTUSERDATA 2
// #define LUA_TBOOLEAN 3
// #define LUA_TSTRING 4
// #define LUA_TTABLE 5
// #define LUA_TFUNCTION 6
// #define LUA_TTHREAD 7

// // stack define
// #define LUA_MINSTACK 20
// #define LUA_STACKSIZE (2 * LUA_MINSTACK)
// #define LUA_EXTRASTACK 5
// // #define LUA_MAXSTACK 15000
// // #define LUA_ERRORSTACK 200
// // #define LUA_MULRET -1
// // #define LUA_MAXCALLS 200

// // ERROR CODE
// #define LUA_OK 0
// #define LUA_ERRERR 1
// #define LUA_ERRMEM 2
// #define LUA_ERRRUN 3

// #define LUA_INTEGER int32
// #define LUA_NUMBER double
// #define LUA_EXTRASPACE sizeof(void *)

// #define LUA_TRY(L, c, a)      \
//     if (_setjmp((c)->b) == 0) \
//     {                         \
//         a                     \
//     }
// #define LUA_THROW(c) longjmp((c)->b, 1)

// #define G(L) ((L)->l_G)

// typedef struct lua_State lua_State;
// typedef LUA_INTEGER lua_Integer;
// typedef LUA_NUMBER lua_Number;
// typedef uint8 lu_byte;

// // lua_CFunction只有一个参数，就是Lua虚拟机的“线程”类型实例，它所有的参数都在“线程”的栈中
// // 它所有的参数都在“线程”的栈中。Light C Function只有一个int类型的返回值，
// // 这个返回值告知调用者，在lua_CFuntion函数被调用完成之后，有多少个返回值还在栈中
// typedef int (*lua_CFunction)(lua_State *L);
// typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

// // 变量的值
// typedef union lua_Value
// {
//     void *p;         // light userdata 类型变量
//     int b;           // bool值
//     lua_Integer i;   // 整数
//     lua_Number n;    // 浮点型变量
//     lua_CFunction f; // Light C Function, 没有upvalue
// } Value;

// // 变量类型
// typedef struct lua_TValue
// {
//     Value value_;
//     int tt_;
// } TValue;

// typedef TValue *StkId;

// struct CallInfo
// {
//     StkId func;     // 函数位于stack中的位置
//     StkId top;      // 被调用函数的栈顶位置
//     int nresult;    // 被调用函数一共要返回多少个值
//     int callstatus; // 函数调用的状态，CIST_LUA（表明调用的是一个lua函数）
//     // 一个函数再调用其他函数时，会产生新的CallInfo实例
//     struct CallInfo *next;     // 新的被调用函数的实例
//     struct CallInfo *previous; // 调用者
// };

// struct lua_longjmp
// {
//     struct lua_longjmp *previous;
//     jmp_buf b;
//     int status;
// };

// typedef struct lua_State
// {
//     StkId stack;
//     StkId stack_last;
//     StkId top;
//     int stack_size;
//     struct lua_longjmp *errorjmp;
//     int status;
//     struct lua_State *next;
//     struct lua_State *previous;
//     struct CallInfo base_ci; // 基础函数调用信息
//     struct CallInfo *ci;     // 当前被调用函数的信息
//     struct global_State *l_G;
//     ptrdiff_t errorfunc; // 两个指针之间的差值（即地址偏移量）
//     int ncalls;
// } lua_State;

// typedef struct global_State
// {
//     struct lua_State *mainthread;
//     lua_Alloc frealloc; // 内存分配/回收函数
//     void *ud;
//     lua_CFunction panic;
// } global_State;

// typedef struct LX
// {
//     lu_byte extra_[LUA_EXTRASPACE]; // lua虚拟机并未用到
//     struct lua_State l;             // 主线程实例
// } LX;

// // lua_global
// typedef struct LG
// {
//     LX l;
//     global_State g;
// } LG;

// // 创建一个Lua虚拟机实例
// struct lua_State *luaL_newstate();
// struct lua_State *lua_newstate(lua_Alloc alloc, void *ud);
// static void stack_init(struct lua_State *L);
// void *luaM_realloc(struct lua_State *L, void *ptr, size_t osize, size_t nsize);
// void luaD_throw(struct lua_State *L, int error);
// void setnilvalue(StkId target);

// static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
// {
//     (void)ud;
//     (void)osize;

//     // printf("l_alloc nsize:%ld\n", nsize);
//     if (nsize == 0)
//     {
//         free(ptr);
//         return NULL;
//     }
//     return realloc(ptr, nsize);
// }

// struct lua_State *luaL_newstate()
// {
//     struct lua_State *L = lua_newstate(&l_alloc, NULL);
//     return L;
// }

// struct lua_State *lua_newstate(lua_Alloc alloc, void *ud)
// {
//     struct global_State *g;
//     struct lua_State *L;
//     struct LG *lg = (struct LG *)(*alloc)(ud, NULL, LUA_TTHREAD, sizeof(struct LG));
//     if (!lg)
//         return NULL;
//     g = &(lg->g);
//     g->ud = ud;
//     g->frealloc = alloc;
//     g->panic = NULL;

//     L = &(lg->l.l);
//     G(L) = g; // L->l_G = g;
//     g->mainthread = L;

//     stack_init(L);
//     return L;
// }

// static void stack_init(struct lua_State *L)
// {
//     L->stack = (StkId)luaM_realloc(L, NULL, 0, LUA_STACKSIZE * sizeof(TValue));
//     L->stack_size = LUA_STACKSIZE;
//     L->stack_last = L->stack + LUA_STACKSIZE - LUA_EXTRASTACK;
//     L->next = L->previous = NULL;
//     L->status = LUA_OK;
//     L->errorjmp = NULL;
//     L->top = L->stack;
//     L->errorfunc = 0;

//     for (int i = 0; i < L->stack_size; i++)
//     {
//         setnilvalue(L->stack + i);
//     }
//     L->top++;

//     L->ci = &L->base_ci;
//     L->ci->func = L->stack;
//     L->ci->top = L->stack + LUA_MINSTACK;
//     L->ci->previous = L->ci->next = NULL;
// }

// void *luaM_realloc(struct lua_State *L, void *ptr, size_t osize, size_t nsize)
// {
//     struct global_State *g = G(L);
//     int oldsize = ptr ? osize : 0;

//     void *ret = (*g->frealloc)(g->ud, ptr, oldsize, nsize);
//     if (ret == NULL)
//     {
//         luaD_throw(L, LUA_ERRMEM);
//     }
//     return ret;
// }

// void luaD_throw(struct lua_State *L, int error)
// {
//     struct global_State *g = G(L);
//     if (L->errorjmp)
//     {
//         L->errorjmp->status = error;
//         LUA_THROW(L->errorjmp);
//     }
//     else
//     {
//         if (g->panic)
//         {
//             (*g->panic)(L);
//         }
//         abort();
//     }
// }

// void setnilvalue(StkId target)
// {
//     target->tt_ = LUA_TNIL;
// }

// int main()
// {
//     return 0;
// }