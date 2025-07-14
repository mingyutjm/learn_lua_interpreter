#ifndef luaobject_h
#define luaobject_h

#include "lua.h"

typedef struct lua_State lua_State;

typedef LUA_INTEGER lua_Integer;
typedef LUA_NUMBER lua_Number;
typedef unsigned char lu_byte;
// lua_CFunction只有一个参数，就是Lua虚拟机的“线程”类型实例，它所有的参数都在“线程”的栈中
// 它所有的参数都在“线程”的栈中。Light C Function只有一个int类型的返回值，
// 这个返回值告知调用者，在lua_CFuntion函数被调用完成之后，有多少个返回值还在栈中
typedef int (*lua_CFunction)(lua_State *L);
typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

// lua number type
#define LUA_NUMINT (LUA_TNUMBER | (0 << 4)) // 整数
#define LUA_NUMFLT (LUA_TNUMBER | (1 << 4)) // 浮点数

// lua function type
#define LUA_TLCL (LUA_TFUNCTION | (0 << 4))
#define LUA_TLCF (LUA_TFUNCTION | (1 << 4)) // light c function
#define LUA_TCCL (LUA_TFUNCTION | (2 << 4))

// string type
#define LUA_LNGSTR (LUA_TSTRING | (0 << 4)) // 长字符串
#define LUA_SHRSTR (LUA_TSTRING | (1 << 4)) // 短字符串

// GCObject
#define CommonHeader       \
    struct GCObject *next; \
    lu_byte tt_;           \
    lu_byte marked
#define LUA_GCSTEPMUL 200

#define luaO_nilobject (&luaO_nilobject_)
#define MAXSHORTSTR 40

typedef struct GCObject
{
    CommonHeader; // 通用头部
} GCObject;

// 变量的值
typedef union lua_Value
{
    GCObject *gc;    // 垃圾回收对象
    void *p;         // light userdata 类型变量
    int b;           // bool值
    lua_Integer i;   // 整数
    lua_Number n;    // 浮点型变量
    lua_CFunction f; // Light C Function, 没有upvalue
} Value;

// 变量类型
typedef struct lua_TValue
{
    Value value_;
    int tt_;
} TValue;

const TValue luaO_nilobject_;

// Lua字符串类型
typedef struct TString
{
    CommonHeader;
    u32 hash;

    // 长字符串时, 0 表示字符串未进行hash，1表示已经经过hash
    // 短字符串时，0 表示需要被gc托管(普通字符串），1表示不会被gc回收(关键字)
    u16 extra;
    u16 shrlen; // 短字符串的长度
    union
    {
        struct TString *hnext; // 只对短字符串有效，发生hash冲突时，指向下一个短字符串
        size_t lnglen;         // 长字符串的长度
    } u;
    char data[0];
} TString;

#endif