#include "stdafx.h"
#include "SharedTable.h"

int main()
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaL_requiref(L, "SharedTable", luaopen_SharedTable, 1);
	luaL_dofile(L, "../lua/testSharedTable.lua");

	lua_State* L2 = luaL_newstate();
	luaL_openlibs(L2);
	luaL_requiref(L2, "SharedTable", luaopen_SharedTable, 1);
	luaL_dofile(L2, "../lua/testSharedTable_in2states.lua");
	lua_close(L2);

	lua_close(L);

	return 0;
}

