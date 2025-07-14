#include "luagc.h"
#include "luamem.h"

#define GCMAXSWEEPGCO 25

// 本质上gray就是全0, 但是以下兼容一些可能不会发生的特殊情况
#define white2gray(o) resetbits((o)->marked, WHITEBITS) // 白位全0, 如果不白, 无效
#define gray2black(o) l_setbit((o)->marked, BLACKBIT)   // 黑位 置1, 如果是白, 那就既白又黑
#define black2gray(o) resetbit((o)->marked, BLACKBIT)   // 黑位 置0, 如果不黑, 无效

#define gettotalbytes(g) (g->totalbytes + g->GCdebt)
#define sweepwholelist(L, list) sweeplist(L, list, MAX_LUMEM)

static l_mem getdebt(struct lua_State *L)
{
    struct global_State *g = G(L);
    l_mem debt = g->GCdebt;
    if (debt <= 0)
        return 0;

    debt = debt / STEPMULADJ + 1; // 除200再向上取整
    debt = debt >= (MAX_LMEM / STEPMULADJ) ? MAX_LMEM : debt * g->GCstepmul;
    return debt;
}

static void setdebt(struct lua_State *L, l_mem debt)
{
    struct global_State *g = G(L);
    lu_mem totalbytes = gettotalbytes(g);

    g->totalbytes = totalbytes - debt;
    g->GCdebt = debt;
}

// 遍历lua_State的栈, 标记栈中用到的值为灰色, 返回整个L占用内存大小
static lu_mem traversethread(struct lua_State *L, struct lua_State *th)
{
    TValue *o = th->stack;
    // 遍历栈中用到的值, 标记为灰色, 并加入 g->gray
    for (; o < th->top; o++)
    {
        markvalue(L, o);
    }

    // lua_State大小等于 本身结构体大小 + stack大小(包含空位) + 函数调用的CallInfo的大小*个数
    return sizeof(struct lua_State) + sizeof(TValue) * th->stack_size + sizeof(struct CallInfo) * th->nci;
}

// 一次标记一个lua_State
static void propagatemark(struct lua_State *L)
{
    struct global_State *g = G(L);
    if (!g->gray)
    {
        return;
    }
    struct GCObject *gco = g->gray;
    gray2black(gco);
    lu_mem size = 0;

    switch (gco->tt_)
    {
    case LUA_TTHREAD:
    {
        struct lua_State *th = gco2th(gco);
        // 把th从g->gray上摘下
        g->gray = th->gclist;
        // atomic状态下不再加入grayagain
        if (g->gcstate != GCSinsideatomic)
        {
            black2gray(gco);
            // 放入g->grayagain
            linkgclist(th, g->grayagain);
        }
        // 遍历th的栈, 标记栈中用到的值为灰色
        size = traversethread(L, th);
    }
    break;
    default:
        break;
    }

    g->GCmemtrav += size;
}

static void propagateall(struct lua_State *L)
{
    struct global_State *g = G(L);
    while (g->gray)
    {
        propagatemark(L);
    }
}

static lu_mem freeobj(struct lua_State *L, struct GCObject *gco)
{
    switch (gco->tt_)
    {
    case LUA_TSTRING:
    {
        lu_mem sz = sizeof(TString);
        luaM_free(L, gco, sz);
        return sz;
    }
    break;
    default:
    {
        lua_assert(0);
    }
    break;
    }
    return 0;
}

static void atomic(struct lua_State *L)
{
    struct global_State *g = G(L);
    g->gray = g->grayagain;
    g->grayagain = NULL;

    g->gcstate = GCSinsideatomic;
    propagateall(L);
    g->currentwhite = otherwhite(g);
}

static struct GCObject **sweeplist(struct lua_State *L, struct GCObject **p, size_t count)
{
    struct global_State *g = G(L);
    // 因为atomic把颜色切换了，这里所以要再切一次。ow是原色
    lu_byte ow = otherwhite(g);
    while (*p != NULL && count > 0)
    {
        lu_byte marked = (*p)->marked;
        // 是否是同一种白 （要销毁的白）
        if (isdeadm(ow, marked))
        {
            struct GCObject *gco = *p;
            *p = (*p)->next;
            g->GCmemtrav += freeobj(L, gco);
        }
        else
        {
            // 后三位置0 （颜色位置0）
            (*p)->marked &= cast(lu_byte, ~(bitmask(BLACKBIT) | WHITEBITS));
            // 重新标记为新白色
            (*p)->marked |= luaC_white(g);
            p = &((*p)->next);
        }
        count--;
    }
    return (*p) == NULL ? NULL : p;
}

static void entersweep(struct lua_State *L)
{
    struct global_State *g = G(L);
    g->gcstate = GCSsweepallgc;
    // 这里为什么要sweeplist 1? 直接  g->sweepgc = &g->allgc; 会有什么问题吗？
    g->sweepgc = sweeplist(L, &g->allgc, 1);
}

static void sweepstep(struct lua_State *L)
{
    struct global_State *g = G(L);
    if (g->sweepgc)
    {
        g->sweepgc = sweeplist(L, g->sweepgc, GCMAXSWEEPGCO);
        g->GCestimate = gettotalbytes(g);

        if (g->sweepgc)
            return;
    }
    g->gcstate = GCSsweepend;
    g->sweepgc = NULL;
}

static void setpause(struct lua_State *L)
{
    struct global_State *g = G(L);
    l_mem estimate = g->GCestimate / GCPAUSE;
    // 这里estimate = g->GCestimate * g->GCstepmul / GCPAUSE 
    // 相当于 g->GCestimate * 2
    estimate = min(MAX_LMEM, estimate * g->GCstepmul);
    // 然后算出debt, = -g->GCestimate
    // 也就是说，下次内存达到上次gc后的内存的两倍时，才会发生gc，也就是又申请了一倍的内存后才会gc
    l_mem debt = g->GCestimate - estimate;
    setdebt(L, debt);
}

// mark root
static void restart_collection(struct lua_State *L)
{
    struct global_State *g = G(L);
    g->gray = NULL;
    g->grayagain = NULL;
    // g->mainthread 链上 g->gray
    markobject(L, g->mainthread);
}

static lu_mem singlestep(struct lua_State *L)
{
    struct global_State *g = G(L);
    switch (g->gcstate)
    {
    case GCSpause:
    {
        g->GCmemtrav = 0;
        restart_collection(L);
        g->gcstate = GCSpropagate;
        return g->GCmemtrav;
    }
    break;
    case GCSpropagate:
    {
        g->GCmemtrav = 0;
        propagatemark(L);
        if (g->gray == NULL)
            g->gcstate = GCSatomic;
        return g->GCmemtrav;
    }
    break;
    case GCSatomic:
    {
        g->GCmemtrav = 0;
        // 到这一步, 还有什么情况会导致g->gray不为NULL? 先战未来!
        if (g->gray)
        {
            propagateall(L);
        }
        atomic(L);
        entersweep(L);
        g->GCestimate = gettotalbytes(g);
        return g->GCmemtrav;
    }
    break;
    case GCSsweepallgc:
    {
        g->GCmemtrav = 0;
        sweepstep(L);
        return g->GCmemtrav;
    }
    break;
    case GCSsweepend:
    {
        g->GCmemtrav = 0;
        g->gcstate = GCSpause;
        return 0;
    }
    break;
    default:
        break;
    }
    return g->GCmemtrav;
}

// public API
GCObject *luaC_newobj(struct lua_State *L, int tt_, size_t size)
{
    struct global_State *g = G(L);
    GCObject *obj = (GCObject *)luaM_realloc(L, NULL, 0, size);
    obj->marked = luaC_white(g);
    obj->next = g->allgc;
    obj->tt_ = tt_;
    g->allgc = obj;
    return obj;
}

void reallymarkobject(struct lua_State *L, struct GCObject *gco)
{
    struct global_State *g = G(L);
    white2gray(gco);

    switch (gco->tt_)
    {
    case LUA_TTHREAD:
    {
        linkgclist(gco2th(gco), g->gray);
    }
    break;
    case LUA_TSTRING:
    { // just for gc test now
        gray2black(gco);
        g->GCmemtrav += sizeof(struct TString);
    }
    break;
    default:
        break;
    }
}

void luaC_step(struct lua_State *L)
{
    struct global_State *g = G(L);
    l_mem debt = getdebt(L);
    do
    {
        l_mem work = singlestep(L);
        debt -= work;
    } while (debt > -GCSTEPSIZE && G(L)->gcstate != GCSpause); // 处理至多debt+GCSTEPSIZE个字节的数据

    // 一轮gc已经完整执行完毕
    if (G(L)->gcstate == GCSpause)
    {
        setpause(L);
    }
    // 还没执行完gc
    else
    {
        debt = debt / g->GCstepmul * STEPMULADJ;
        setdebt(L, debt);
    }
}

void luaC_freeallobjects(struct lua_State *L)
{
    struct global_State *g = G(L);
    g->currentwhite = WHITEBITS; // all gc objects must reclaim
    sweepwholelist(L, &g->allgc);
}
