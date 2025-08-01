#ifndef lua_h
#define lua_h

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>

#if defined(LLONG_MAX)
#define LUA_INTEGER long long
#define LUA_NUMBER double
#else
#define LUA_INTEGER int
#define LUA_NUMBER float
#endif

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define lua_assert(c) ((void)0)
#define check_exp(c, e) (lua_assert(c), e)

// ERROR CODE
#define LUA_OK 0
#define LUA_ERRERR 1
#define LUA_ERRMEM 2
#define LUA_ERRRUN 3

// error tips
#define LUA_ERROR(L, s) printf("LUA ERROR:%s", s);

// basic object type
#define LUA_TNONE (-1)
#define LUA_TNIL 0
#define LUA_TNUMBER 1
#define LUA_TLIGHTUSERDATA 2
#define LUA_TBOOLEAN 3
#define LUA_TSTRING 4
#define LUA_TTABLE 5
#define LUA_TTHREAD 6
#define LUA_TFUNCTION 7

// stack define
#define LUA_MINSTACK 20
#define LUA_STACKSIZE (2 * LUA_MINSTACK)
#define LUA_EXTRASTACK 5
#define LUA_MAXSTACK 15000
#define LUA_ERRORSTACK 200
#define LUA_MULRET -1
#define LUA_MAXCALLS 200

#define cast(t, exp) ((t)(exp))
// #define savestack(L, o) ((o) - (L)->stack)
#define diststack(L, o) ((o) - (L)->stack) // 到栈底部的距离
#define restorestack(L, o) ((L)->stack + (o))
#define ptr2uint(p) ((uint32_t)((size_t)p & UINT_MAX))
#define novariant(o) ((o)->tt_ & 0xf)   // 看最后四位，获取大类类型

// mem define
typedef size_t lu_mem;
typedef ptrdiff_t l_mem;

#define MAX_LUMEM ((lu_mem)(~(lu_mem)0))
#define MAX_LMEM (MAX_LUMEM >> 1)

#endif