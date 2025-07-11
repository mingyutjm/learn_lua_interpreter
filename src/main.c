// #include "luaaux.h"
#include "unity.c"
#include <time.h>

// \src\luaaux.c ..\src\luado.c ..\src\luamem.c ..\src\luastate.c

static int add_op(struct lua_State *L)
{
    int left = luaL_tointeger(L, -2);
    int right = luaL_tointeger(L, -1);
    luaL_pushinteger(L, left + right);
    // luaD_throw(L, LUA_ERRRUN);
    return 1;
}

#define ELEMENTNUM 5

void p2_test_main()
{
    struct lua_State *L = luaL_newstate();

    int i = 0;
    for (; i < ELEMENTNUM; i++)
    {
        luaL_pushnil(L);
    }

    int start_time = time(NULL);
    int end_time = time(NULL);
    size_t max_bytes = 0;
    struct global_State *g = G(L);
    int j = 0;
    for (; j < 50000; j++)
    {
        TValue *o = luaL_index2addr(L, (j % ELEMENTNUM) + 1);
        struct GCObject *gco = luaC_newobj(L, LUA_TSTRING, sizeof(TString));
        o->value_.gc = gco;
        o->tt_ = LUA_TSTRING;
        luaC_checkgc(L);

        if ((g->totalbytes + g->GCdebt) > max_bytes)
        {
            max_bytes = g->totalbytes + g->GCdebt;
        }

        if (j % 1000 == 0)
        {
            printf("timestamp:%d totalbytes:%f kb \n", (int)time(NULL), (float)(g->totalbytes + g->GCdebt) / 1024.0f);
        }
    }
    end_time = time(NULL);
    printf("finish test start_time:%d end_time:%d max_bytes:%f kb \n", start_time, end_time, (float)max_bytes / 1024.0f);

    luaL_close(L);
}

int main(int argc, char **argv)
{
    // struct lua_State *L = luaL_newstate();
    // luaL_pushcfunction(L, &add_op);
    // luaL_pushinteger(L, 1);
    // luaL_pushinteger(L, 1);
    // luaL_pcall(L, 2, 1); // 虚拟机实例 L, 被调用函数有几个参数，几个返回值，

    // int result = luaL_tointeger(L, -1);
    // printf("result is %d\n", result);
    // luaL_pop(L);
    // printf("final stack size %d\n", luaL_stacksize(L));

    // luaL_close(L);

    p2_test_main();

    system("pause");
    return 0;
}