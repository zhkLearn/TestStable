#include "stdafx.h"
#include "stable.h"
#include <string>
#include <iostream>
using namespace std;

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 16


void stack_dump(lua_State* l)
{
	int i;
	int top = lua_gettop(l);

	printf("%d items in stack 0x%x, from top to bottom:\n", top, l);

	for (i = top; i >= 1; i--)
	{
		/* repeat for each level */
		int t = lua_type(l, i);
		switch (t)
		{
		case LUA_TNIL:  /* nils */
			printf("	nil\n");
			break;
		case LUA_TSTRING:  /* strings */
			printf("	string   : '%s'\n", lua_tostring(l, i));
			break;
		case LUA_TBOOLEAN:  /* booleans */
			printf("	boolean  : %s\n", lua_toboolean(l, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:  /* numbers */
			printf("	number   : %g\n", lua_tonumber(l, i));
			break;
		case LUA_TTABLE:  /* tables */
			printf("	table    : 0x%x\n", lua_topointer(l, i));
			break;
		case LUA_TFUNCTION:  /* functions */
			printf("	function : 0x%x\n", lua_topointer(l, i));
			break;
		case LUA_TUSERDATA:  /* UserData */
			printf("	userdata : 0x%x\n", lua_topointer(l, i));
			break;
		case LUA_TLIGHTUSERDATA:  /* LightUserData */
			printf("	luserdata: 0x%x\n", lua_topointer(l, i)); 
			break;
		case LUA_TTHREAD:  /* LightUserData */
			printf("	thread   : 0x%x\n", lua_topointer(l, i));
			break;
		default:  /* other values */
			{
				const char* str = lua_tostring(l, i);
				if (str)
				{
					printf("	%s: %s\n", lua_typename(l, t), lua_tostring(l, i));
				}
				else
				{
					printf("	%s: 0x%x\n", lua_typename(l, t), lua_topointer(l, i));
				}
				break;
			}
		}
	}
	printf("\n");  /* end the listing */
}

static void _getvalue(lua_State* L, int ttype, UTable_value* tv)
{
	switch (ttype)
	{
	case ST_NIL:
		lua_pushnil(L);
		break;

	case ST_NUMBER:
		lua_pushnumber(L, tv->n);
		break;

	case ST_BOOLEAN:
		lua_pushboolean(L, tv->b);
		break;

	case ST_ID:
		lua_pushlightuserdata(L, (void *)(uintptr_t)tv->id);
		break;

	case ST_STRING:
		stable_value_string(tv, (table_setstring_func)lua_pushlstring, L);
		break;

	case ST_TABLE:
		lua_pushlightuserdata(L, tv->p);
		break;

	default:
		luaL_error(L, "Invalid stable type %d", ttype);
	}
}

static int _get(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	int type = lua_type(L, 2);
	int idx;
	int ttype;
	union UTable_value tv;
	const char *key;
	size_t sz;

	switch (type)
	{
	case LUA_TNUMBER:
		idx = lua_tointeger(L, 2);
		if (idx <= 0)
		{
			return luaL_error(L, "Unsupported index %d", idx);
		}
		ttype = stable_type(t, NULL, idx - 1, &tv);
		break;

	case LUA_TSTRING:
		key = lua_tolstring(L, 2, &sz);
		ttype = stable_type(t, key, sz, &tv);
		break;

	default:
		return luaL_error(L, "Unsupported key type %s", lua_typename(L, type));
	}

	_getvalue(L, ttype, &tv);
	return 1;
}

static void _error(lua_State* L, const char *key, size_t sz, int type)
{
	if (key == NULL)
	{
		luaL_error(L, "Can't set %d with type %s", (int)sz, lua_typename(L, type));
	}
	else
	{
		luaL_error(L, "Can't set %s with type %s", key, lua_typename(L, type));
	}
}

static const char* _get_key(lua_State* L, int key_idx, size_t *sz_idx, bool fromLua = true)
{
	int type = lua_type(L, key_idx);
	const char *key = NULL;
	size_t sz;

	switch (type)
	{
	case LUA_TNUMBER:
		sz = lua_tointeger(L, key_idx);
		if (sz <= 0)
		{
			luaL_error(L, "Unsupported index %lu", sz);
		}

		key = NULL;
		if (fromLua)
		{
			sz--;
		}
		*sz_idx = sz;
		break;

	case LUA_TSTRING:
		key = lua_tolstring(L, key_idx, sz_idx);
		break;

	default:
		luaL_error(L, "Unsupported key type %s", lua_typename(L, type));
	}

	return key;
}

static void _set_value(lua_State* L, STable* t, const char *key, size_t sz, int idx);

static bool traversal_table(lua_State* L, STable* t, int idx)
{
	lua_pushvalue(L, idx);
	lua_pushnil(L);

	while (lua_next(L, -2) != 0)
	{
		size_t sz = 0;
		const char * key = _get_key(L, -2, &sz, false);
		_set_value(L, t, key, sz, -1);

		lua_pop(L, 1);
	}
	lua_pop(L, 2);

	return true;
}

static void _set_value(lua_State* L, STable* t, const char *key, size_t sz, int idx)
{
	int type = lua_type(L, idx);

#ifdef _DEBUG
	const char* typeName = nullptr;
	switch (type)
	{
	case LUA_TNUMBER:
		typeName = "number";
		break;
	case LUA_TBOOLEAN:
		typeName = "boolean";
		break;
	case LUA_TSTRING:
		typeName = "string";
		break;
	case LUA_TTABLE:
		typeName = "table";
		break;
	case LUA_TUSERDATA:
		typeName = "userdata";
		break;
	case LUA_TLIGHTUSERDATA:
		typeName = "luserdata";
		break;
	default:
		typeName = lua_typename(L, type);
		break;
	}

	printf("_set_value, L: 0x%x, STable: 0x%x, Key: %s, Type: %s, Size: %d, Index: %d\n", L, t, key, typeName, sz, idx);
	stack_dump(L);
#endif

	int r = 1;
	switch (type)
	{
	case LUA_TNUMBER:
		r = stable_setnumber(t, key, sz, lua_tonumber(L, idx));
		break;

	case LUA_TBOOLEAN:
		r = stable_setboolean(t, key, sz, lua_toboolean(L, idx) != 0);
		break;

	case LUA_TSTRING:
	{
		size_t len;
		const char* str = lua_tolstring(L, idx, &len);
		r = stable_setstring(t, key, sz, str, len);
		break;
	}

	case LUA_TTABLE:
	{
		STable * tChild = stable_create();
		r = stable_settable(t, key, sz, tChild);
		if (!r)
		{
			if (traversal_table(L, tChild, idx))
			{
				lua_pushlightuserdata(L, tChild);
#ifdef _DEBUG
				printf("lua_pushlightuserdata: 0x%x\n", tChild);
#endif
			}
		}
		break;
	}

	case LUA_TLIGHTUSERDATA:
		r = stable_setid(t, key, sz, (uint64_t)(uintptr_t)lua_touserdata(L, idx));
		break;

	default:
		luaL_error(L, "Unsupported value type %s", lua_typename(L, type));
	}

	if (r)
	{
		_error(L, key, sz, type);
	}

}

static int _settable(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	size_t sz;
	const char* key = _get_key(L, 2, &sz);
	if (stable_settable(t, key, sz, (STable *)lua_touserdata(L, 3)))
	{
		_error(L, key, sz, LUA_TLIGHTUSERDATA);
	}

	return 0;
}

static int _set(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	size_t sz;
	const char * key = _get_key(L, 2, &sz);
	_set_value(L, t, key, sz, 3);
	return 0;
}

static int _iter_stable_array(lua_State* L)
{
	int idx = luaL_checkinteger(L, 2);
	lua_pushinteger(L, idx + 1);
	union UTable_value v;
	int t = stable_type((STable *)lua_touserdata(L, 1), NULL, idx, &v);
	if (t == ST_NIL)
	{
		return 0;
	}

	_getvalue(L, t, &v);
	return 2;
}

static int _ipairs(lua_State* L)
{
	lua_pushcfunction(L, _iter_stable_array);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0);
	return 3;
}

/*
	uv1:  array_part
	uv2:  hash_part
	uv3:  position
	uv4:  userdata: keys
	lightuserdata: stable
	string key

	string nextkey
	value
 */
static int _next_key(lua_State* L, STable *t)
{
	int hash_part = lua_tointeger(L, lua_upvalueindex(2));
	if (hash_part == 0)
	{
		return 0;
	}

	int position = lua_tointeger(L, lua_upvalueindex(3));
	if (position >= hash_part)
	{
		lua_pushinteger(L, 0);
		lua_replace(L, lua_upvalueindex(3));
		return 0;
	}

	lua_pushinteger(L, position + 1);
	lua_replace(L, lua_upvalueindex(3));

	STable_key* keys = (STable_key*)lua_touserdata(L, lua_upvalueindex(4));
	lua_pushlstring(L, keys[position].key, keys[position].sz_idx);

	union UTable_value tv;
	int ttype = stable_type(t, keys[position].key, keys[position].sz_idx, &tv);
	_getvalue(L, ttype, &tv);

	return 2;
}

static int _next_index(lua_State* L, STable *t, int prev_index)
{
	int array_part = lua_tointeger(L, lua_upvalueindex(1));
	if (prev_index >= array_part)
	{
		return _next_key(L, t);
	}

	int i;
	int ttype;
	union UTable_value tv;
	for (i = prev_index; i < array_part; i++)
	{
		ttype = stable_type(t, NULL, i, &tv);
		if (ttype == ST_NIL)
		{
			continue;
		}

		lua_pushinteger(L, i + 1);
		_getvalue(L, ttype, &tv);
		return 2;
	}

	return _next_key(L, t);
}

static int _next_stable(lua_State* L)
{
	STable *t = (STable *)lua_touserdata(L, 1);
	int type = lua_type(L, 2);

	switch (type)
	{
	case LUA_TNIL:
		return _next_index(L, t, 0);
		break;

	case LUA_TNUMBER:
		return _next_index(L, t, lua_tointeger(L, 2));

	case LUA_TSTRING:
		return _next_key(L, t);

	default:
		return luaL_error(L, "Invalid next key");
	}
}

static int _pairs(lua_State* L)
{
	STable *t = (STable *)lua_touserdata(L, 1);
	size_t cap = stable_cap(t);
	STable_key* keys = (STable_key*)malloc(cap * sizeof(*keys));
	int size = stable_keys(t, keys, cap);
	if (size == 0)
	{
		lua_pushinteger(L, 0);
		lua_pushinteger(L, 0);
		lua_pushinteger(L, 0);
		lua_pushnil(L);
	}
	else
	{
		int array_part = 0;
		int hash_part = 0;
		int i = 0;
		for (i = 0; i < size; i++)
		{
			STable_key* key = &keys[i];
			if (key->key)
			{
				break;
			}
		}

		if (i == 0)
		{
			hash_part = size;
		}
		else
		{
			array_part = keys[i - 1].sz_idx + 1;
			hash_part = size - i;
		}

		lua_pushinteger(L, array_part);
		lua_pushinteger(L, hash_part);
		lua_pushinteger(L, 0);

		if (hash_part > 0)
		{
			void * ud = lua_newuserdata(L, hash_part * sizeof(struct STable_key));
			memcpy(ud, keys + i, hash_part * sizeof(struct STable_key));
		}
		else
		{
			lua_pushnil(L);
		}
	}

	lua_pushcclosure(L, _next_stable, 4);
	free(keys);
	lua_pushvalue(L, 1);
	return 2;
}

static int _init_metaTable(lua_State* L)
{
	luaL_Reg lib[] =
	{
		{ "__index",	_get },
		{ "__newindex",	_set },
		{ "__pairs",	_pairs },
		{ "__ipairs",	_ipairs },
		{ NULL,			NULL },
	};

	lua_pushlightuserdata(L, NULL);
	int m = lua_getmetatable(L, -1);
	if (m == 0)
	{
		luaL_newlibtable(L, lib);
	}

	luaL_setfuncs(L, lib, 0);
	lua_setmetatable(L, -2);
	lua_pop(L, 1);

	return 0;
}

static int _create(lua_State* L)
{
	STable* t = stable_create();
	lua_pushlightuserdata(L, t);
	return 1;
}

static int _release(lua_State* L)
{
	STable ** t = (STable **)lua_touserdata(L, 1);
	stable_release(*t);
	return 0;
}

static int _grab(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	STable* t = (STable *)lua_touserdata(L, 1);
	stable_grab(t);
	STable ** ud = (STable **)lua_newuserdata(L, sizeof(STable *));
	*ud = t;
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	return 1;
}

static int _decref(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	stable_release(t);
	return 0;
}

static int _getref(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	int ref = stable_getref(t);
	lua_pushinteger(L, ref);
	return 1;
}

static int _incref(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	stable_grab(t);
	return 0;
}

static int _dump(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	stable_dump(t, 0);
	return 0;
}

int luaopen_stable_raw(lua_State* L)
{
	luaL_checkversion(L);

	luaL_Reg l[] =
	{
		{ "create",		_create },
		{ "decref",		_decref },
		{ "incref",		_incref },
		{ "getref",		_getref },
		{ "get",		_get },
		{ "set",		_set },
		{ "settable",	_settable },
		{ "pairs",		_pairs },
		{ "ipairs",		_ipairs },
		{ "init",		_init_metaTable },
		{ "dump",		_dump },
		{ NULL,			NULL },
	};

	luaL_newlib(L, l);

	lua_createtable(L, 0, 1);

	lua_pushcfunction(L, _release);
	lua_setfield(L, -2, "__gc");

	lua_pushcclosure(L, _grab, 1);
	lua_setfield(L, -2, "grab");

	return 1;
}
