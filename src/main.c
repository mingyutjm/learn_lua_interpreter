// #include "luaaux.h"
#include "unity.c"

// \src\luaaux.c ..\src\luado.c ..\src\luamem.c ..\src\luastate.c

static int add_op(struct lua_State *L)
{
    int left = luaL_tointeger(L, -2);
    int right = luaL_tointeger(L, -1);
    luaL_pushinteger(L, left + right);
    // luaD_throw(L, LUA_ERRRUN);
    return 1;
}

int main(int argc, char **argv)
{
    struct lua_State *L = luaL_newstate();
    luaL_pushcfunction(L, &add_op);
    luaL_pushinteger(L, 1);
    luaL_pushinteger(L, 1);
    luaL_pcall(L, 2, 1);  // 虚拟机实例 L, 被调用函数有几个参数，几个返回值，

    int result = luaL_tointeger(L, -1);
    printf("result is %d\n", result);
    luaL_pop(L);
    printf("final stack size %d\n", luaL_stacksize(L));

    luaL_close(L);
    system("pause");
    return 0;
}