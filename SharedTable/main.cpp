#include "stdafx.h"
#include "SharedTable.h"

int main()
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaL_requiref(L, "SharedTable", luaopen_SharedTable, 1);
	if (luaL_dofile(L, "../lua/testSharedTable.lua"))
	{
		printf("%s\n", lua_tostring(L, -1));
	}

	lua_State* L2 = luaL_newstate();
	luaL_openlibs(L2);
	luaL_requiref(L2, "SharedTable", luaopen_SharedTable, 1);
	if (luaL_dofile(L2, "../lua/testSharedTable_in2states.lua"))
	{
		printf("%s\n", lua_tostring(L2, -1));
	}
	lua_close(L2);

	lua_close(L);

	return 0;
}

