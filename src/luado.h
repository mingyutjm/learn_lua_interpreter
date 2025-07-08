#ifndef luado_h
#define luado_h

#include "luastate.h"

void seterrobj(struct lua_State* L, int error);
void luaD_checkstack(struct lua_State* L, int need);
void luaD_growstack(struct lua_State* L, int size);
void luaD_throw(struct lua_State *L, int error);

int luaD_call(struct lua_State *L, StkId func, int nresult);
int luaD_precall(struct lua_State *L, StkId func, int nresult);
int luaD_poscall(struct lua_State* L, StkId first_result, int nresult);

#endif