#include "luamem.h"
#include "luado.h"

void *luaM_realloc(struct lua_State *L, void *ptr, size_t osize, size_t nsize)
{
    struct global_State *g = G(L);
    int oldsize = ptr ? osize : 0;

    void *ret = (*g->frealloc)(g->ud, ptr, oldsize, nsize);
    if (ret == NULL && nsize > 0)
    {
        luaD_throw(L, LUA_ERRMEM);
    }

    // 对于新建的对象，oldsize==0, GCdebt需要加上nsize
    // 对于删除的对象，nsize==0, GCdebt需要减去oldsize
    // GCdebt += 内存变化的量
    g->GCdebt = g->GCdebt + (nsize - oldsize);
    return ret;
}