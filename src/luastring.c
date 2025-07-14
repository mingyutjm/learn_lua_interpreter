#include "luastring.h"
#include "luamem.h"

#define MEMERRMSG "not enough memory"
#define MINSTRTABLESIZE 128
#define lmod(hash, size) ((hash) & (size - 1)) // size是2^n-1, 就是全1

void luaS_init(struct lua_State *L)
{
    struct global_State *g = G(L);
    g->strt.nuse = 0;
    g->strt.size = 0;
    g->strt.hash = NULL;
    luaS_resize(L, MINSTRTABLESIZE);

    g->memerrmsg = luaS_newlstr(L, MEMERRMSG, strlen(MEMERRMSG));
    luaC_fix(L, obj2gco(g->memerrmsg));
    // 缓存数组全部初始化成 memerrmsg
    for (int i = 0; i < STRCACHE_M; i++)
    {
        for (int j = 0; j < STRCACHE_N; j++)
            g->strcache[i][j] = g->memerrmsg;
    }
}

// nsize必须是2^n
int luaS_resize(struct lua_State *L, u32 nsize)
{
    struct global_State *g = G(L);
    u32 osize = g->strt.size;
    // 扩容
    if (nsize > osize)
    {
        luaM_reallocvector(L, g->strt.hash, osize, nsize, TString *);
        // 新开辟的空间置空
        for (int i = osize; i < nsize; i++)
        {
            g->strt.hash[i] = NULL;
        }
    }

    // rehash
    for (int i = 0; i < osize; i++)
    {
        struct TString *ts = g->strt.hash[i];
        g->strt.hash[i] = NULL;

        while (ts)
        {
            struct TString *old_next = ts->u.hnext;
            // ts的新hash
            u32 hash = lmod(ts->hash, nsize);
            // 头插 新hash链表
            ts->u.hnext = g->strt.hash[hash];
            g->strt.hash[hash] = ts;
            //
            ts = old_next;
        }
    }

    // 容量收缩
    if (nsize < osize)
    {
        // 确保要回收的空间是无人占用的？
        lua_assert(g->strt.hash[nsize] == NULL && g->strt.hash[osize - 1] == NULL);
        luaM_reallocvector(L, g->strt.hash, osize, nsize, TString *);
    }
    g->strt.size = nsize;

    return g->strt.size;
}

u32 luaS_hash(struct lua_State *L, const char *str, u32 l, u32 h)
{
    h = h ^ l;
    u32 step = (l >> 5) + 1;
    for (int i = 0; i < l; i = i + step)
    {
        h ^= (h << 5) + (h >> 2) + cast(lu_byte, str[i]);
    }
    return h;
}

u32 luaS_hashlongstr(struct lua_State *L, struct TString *ts)
{
    if (ts->extra == 0)
    {
        ts->hash = luaS_hash(L, getstr(ts), ts->u.lnglen, G(L)->seed);
        ts->extra = 1;
    }
    return ts->hash;
}

static struct TString *createstrobj(struct lua_State *L, const char *str, int type, u32 l, u32 hash)
{
    size_t total_size = sizelstring(l);

    // 新建一个对象
    struct GCObject *o = luaC_newobj(L, type, total_size);
    // 转成TString
    struct TString *ts = gco2ts(o);
    // 复制str到ts->data
    memcpy(getstr(ts), str, l * sizeof(char));
    getstr(ts)[l] = '\0';
    ts->extra = 0;

    // 短字符串
    if (type == LUA_SHRSTR)
    {
        ts->shrlen = cast(lu_byte, l);
        ts->hash = hash;
        ts->u.hnext = NULL;
    }
    // 长字符串
    else if (type == LUA_LNGSTR)
    {
        ts->hash = 0;
        ts->u.lnglen = l;
    }
    else
    {
        lua_assert(0);
    }

    return ts;
}

// 短字符串
static struct TString *internalstr(struct lua_State *L, const char *str, u32 l)
{
    struct global_State *g = G(L);
    struct stringtable *tb = &g->strt;
    u32 h = luaS_hash(L, str, l, g->seed);
    // 找出hash对应的链表
    struct TString **list = &tb->hash[lmod(h, tb->size)];

    // 判断hash链表中是否有相同的短字符串
    for (struct TString *ts = *list; ts; ts = ts->u.hnext)
    {
        // 找到了相同的短字符串
        if (ts->shrlen == l && (memcmp(getstr(ts), str, l * sizeof(char)) == 0))
        {
            // 刷新字符串的gc状态，白色标为otherwhite，避免本次被gc
            if (isdead(g, ts))
            {
                changewhite(ts);
            }
            return ts;
        }
    }

    // hash表需要扩容
    if (tb->nuse >= tb->size && tb->size < INT_MAX / 2)
    {
        // 扩容
        luaS_resize(L, tb->size * 2);
        // 扩容后的地址变了，重新拿到链表
        list = &tb->hash[lmod(h, tb->size)];
    }

    // 创建ts
    struct TString *ts = createstrobj(L, str, LUA_SHRSTR, l, h);
    // 头插链表
    ts->u.hnext = *list;
    *list = ts;
    // hash表元素数量+1
    tb->nuse++;
    return ts;
}

struct TString *luaS_newlstr(struct lua_State *L, const char *str, u32 l)
{
    if (l <= MAXSHORTSTR)
    {
        return internalstr(L, str, l);
    }
    else
    {
        return luaS_createlongstr(L, str, l);
    }
}

struct TString *luaS_new(struct lua_State *L, const char *str, u32 l)
{
    // 直接把指针地址转成u32
    u32 hash = ptr2uint(str);
    int i = hash % STRCACHE_M;
    // 一个hash存STRCACHE_N缓存
    for (int j = 0; j < STRCACHE_N; j++)
    {
        struct TString *ts = G(L)->strcache[i][j];
        // 如果相等，直接返回
        if (strcmp(getstr(ts), str) == 0)
        {
            return ts;
        }
    }

    // 缓存第一位放到第二位
    for (int j = STRCACHE_N - 1; j > 0; j--)
    {
        G(L)->strcache[i][j] = G(L)->strcache[i][j - 1];
    }

    // 新字符串放到第一位
    G(L)->strcache[i][0] = luaS_newlstr(L, str, l);
    return G(L)->strcache[i][0];
}

// hash链表中删除ts
void luaS_remove(struct lua_State *L, struct TString *ts)
{
    struct global_State *g = G(L);
    struct TString **list = &g->strt.hash[lmod(ts->hash, g->strt.size)];
    for (struct TString *o = *list; o; o = o->u.hnext)
    {
        if (o == ts)
        {
            *list = o->u.hnext;
            break;
        }
        list = &(*list)->u.hnext;
    }
}

void luaS_clearcache(struct lua_State *L)
{
    struct global_State *g = G(L);
    // 把white重置为errmsg
    for (int i = 0; i < STRCACHE_M; i++)
    {
        for (int j = 0; j < STRCACHE_N; j++)
        {
            if (iswhite(g->strcache[i][j]))
            {
                g->strcache[i][j] = g->memerrmsg;
            }
        }
    }
}

struct TString *luaS_createlongstr(struct lua_State *L, const char *str, u32 l)
{
    return createstrobj(L, str, LUA_LNGSTR, l, G(L)->seed);
}

int luaS_eqshrstr(struct lua_State *L, struct TString *a, struct TString *b)
{
    return (a == b) || (a->shrlen == b->shrlen && strcmp(getstr(a), getstr(b)) == 0);
}

int luaS_eqlngstr(struct lua_State *L, struct TString *a, struct TString *b)
{
    return (a == b) || (a->u.lnglen == b->u.lnglen && strcmp(getstr(a), getstr(b)) == 0);
}
