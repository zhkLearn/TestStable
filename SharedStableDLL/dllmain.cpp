// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

extern "C"
{
#include <lua.h>  
#include <lualib.h>  
#include <lauxlib.h>  
}

extern int luaopen_SharedTable(lua_State *L);

extern "C" __declspec(dllexport)
int luaopen_SharedStableDLL(lua_State* L)
{
	luaopen_SharedTable(L);
	return 1;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

