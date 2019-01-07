#include "stdafx.h"
#include "stable.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern int test_mt();


static void test(struct STable *root)
{
    stable_setnumber(root, TINDEX(10), 100);

    struct STable * sub = stable_create();
    stable_settable(root, TKEY("hello"), sub);
    stable_setstring(sub, TINDEX(0), TKEY("world"));
    stable_setstring(sub, TINDEX(1), TKEY("This is a test string."));
    stable_setstring(sub, TKEY("key"), TKEY("value"));
	stable_dump(root, 0, true);
}

extern "C"
{
#include <lua.h>  
#include <lualib.h>  
#include <lauxlib.h>  
}

extern int luaopen_stable_raw(lua_State *L);

int main()
{
    STable * t = stable_create();
    test(t);
    stable_release(t);

	//test_mt();

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaL_requiref(L, "sraw", luaopen_stable_raw, 1);
	luaL_dofile(L, "../lua/testRaw.lua");
	lua_close(L);

    return 0;
}

