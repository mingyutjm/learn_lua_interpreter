#ifndef luastring_h
#define luastring_h

#include "luastate.h"
#include "luagc.h"

#define sizelstring(l) (sizeof(TString) + (l + 1) * sizeof(char))   // +1 == '\0'
#define getstr(ts) (ts->data)

void luaS_init(struct lua_State *L);
int luaS_resize(struct lua_State *L, u32 nsize);

struct TString *luaS_newlstr(struct lua_State *L, const char *str, u32 l);
struct TString *luaS_new(struct lua_State *L, const char *str, u32 l);
void luaS_remove(struct lua_State *L, struct TString *ts);

int luaS_eqshrstr(struct lua_State *L, struct TString *a, struct TString *b);
int luaS_eqlngstr(struct lua_State *L, struct TString *a, struct TString *b);

void luaS_clearcache(struct lua_State *L);

u32 luaS_hash(struct lua_State *L, const char *str, u32 l, u32 h);
u32 luaS_hashlongstr(struct lua_State *L, struct TString *ts);
struct TString *luaS_createlongstr(struct lua_State *L, const char *str, u32 l);

#endif