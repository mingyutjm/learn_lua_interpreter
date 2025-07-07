#include "lua.c"

static int add_op(struct lua_State *L)
{
    int left = luaL_tointeger(L, -2);
    int right = luaL_tointeger(L, -1);
    luaL_pushinteger(L, left + right);
    return 0;
}

int main(int argc, char **argv)
{
    struct lua_State *L = luaL_newstate();
    luaL_pushcfunction(L, &add_op);
    luaL_pushinteger(L, 1);
    luaL_pushinteger(L, 1);
    lua_pcall(L, 2, 1, 0);

    int result = luaL_tointeger(L, -1);
    printf("result is %d\n", result);
    luaL_pop(L);
    printf("final stack size %d\n", luaL_stacksize(L));

    luaL_close(L);
    return 0;
}