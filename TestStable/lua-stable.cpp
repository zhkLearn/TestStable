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
			printf("	%5d  %5d	nil\n", i - top - 1, i);
			break;
		case LUA_TSTRING:  /* strings */
			printf("	%5d  %5d	string   : '%s'\n", i - top - 1, i, lua_tostring(l, i));
			break;
		case LUA_TBOOLEAN:  /* booleans */
			printf("	%5d  %5d	boolean  : %s\n", i - top - 1, i, lua_toboolean(l, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:  /* numbers */
			printf("	%5d  %5d	number   : %g\n", i - top - 1, i, lua_tonumber(l, i));
			break;
		case LUA_TTABLE:  /* tables */
			printf("	%5d  %5d	table    : 0x%x\n", i - top - 1, i, lua_topointer(l, i));
			break;
		case LUA_TFUNCTION:  /* functions */
			printf("	%5d  %5d	function : 0x%x\n", i - top - 1, i, lua_topointer(l, i));
			break;
		case LUA_TUSERDATA:  /* UserData */
			printf("	%5d  %5d	userdata : 0x%x\n", i - top - 1, i, lua_topointer(l, i));
			break;
		case LUA_TLIGHTUSERDATA:  /* LightUserData */
			printf("	%5d  %5d	luserdata: 0x%x\n", i - top - 1, i, lua_topointer(l, i));
			break;
		case LUA_TTHREAD:  /* LightUserData */
			printf("	%5d  %5d	thread   : 0x%x\n", i - top - 1, i, lua_topointer(l, i));
			break;
		default:  /* other values */
			{
				const char* str = lua_tostring(l, i);
				if (str)
				{
					printf("	%5d  %5d	%s: %s\n", i - top - 1, i, lua_typename(l, t), lua_tostring(l, i));
				}
				else
				{
					printf("	%5d  %5d	%s: 0x%x\n", i - top - 1, i, lua_typename(l, t), lua_topointer(l, i));
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

static int _share(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	
	//TODO

	return 0;
}

static int _acquire(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	if (!name)
	{
		luaL_error(L, "need a valid table name");
	}

	//TODO

	STable* t = stable_create();
	stable_setnumber(t, TKEY("test"), 100);
	lua_pushlightuserdata(L, t);

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
	lua_checkstack(L, 1);
	lua_pushnil(L);

#ifdef _DEBUG
	printf("traversal_table(L: 0x%x, STable: 0x%x)\n", L, t);
	printf("lua_pushnil\n");
	printf("lua_next\n");
#endif

	//lua_next从栈顶弹出一个键，然后把索引指定的表中的一个键-值对压入栈。如果表中以无更多元素，那么lua_next将返回0（什么也不压栈）。
	//在遍历时，不要直接对键调用lua_tolstring，除非你知道这个键一定是一个字符串。对非字符串调用lua_tolstring有可能改变给定索引位置的值；
	//这会对下一次调用lua_next造成影响。
	while (lua_next(L, -2) != 0)
	{
		size_t sz = 0;
		const char * key = _get_key(L, -2, &sz, false);
		_set_value(L, t, key, sz, -1);

		lua_pop(L, 1);

#ifdef _DEBUG
		printf("lua_pop: 1\n");
		printf("lua_next\n");
#endif

	}
	lua_pop(L, 1);

#ifdef _DEBUG
	printf("lua_pop: 1\n");
	printf("traversal_table(L: 0x%x, STable: 0x%x) end\n", L, t);
	stack_dump(L);
#endif

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
				lua_checkstack(L, 1);
				lua_pushnil(L);	// place holder for to be popped in nested traversal_table()
#ifdef _DEBUG
				printf("lua_pushnil\n");
				stack_dump(L);
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
	stack_dump(L);

	STable* t = (STable *)lua_touserdata(L, 1);
	if (!t)
	{
		luaL_error(L, "table is nil");
	}
	size_t sz;
	const char* key = _get_key(L, 2, &sz);

	STable* tValue = (STable *)lua_touserdata(L, 3);
	if (!tValue)
	{
		luaL_error(L, "value is not a table");
	}

	if (stable_settable(t, key, sz, tValue))
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

	lua_pushlightuserdata(L, NULL);		//0x0

	int m = lua_getmetatable(L, -1);
	if (m == 0)
	{
		luaL_newlibtable(L, lib);		//0x0, table_lib
	}

	luaL_setfuncs(L, lib, 0);			//0x0, table_lib
	lua_setmetatable(L, -2);			//0x0

	lua_pop(L, 1);						//

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

static int _incref(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	stable_grab(t);
	return 0;
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

static int _dump(lua_State* L)
{
	STable* t = (STable *)lua_touserdata(L, 1);
	stable_dump(t, 0);
	return 0;
}

int luaopen_stable_raw(lua_State* L)
{
	luaL_Reg l[] =
	{
		{ "create",		_create },
		{ "incref",		_incref },
		{ "decref",		_decref },
		{ "getref",		_getref },
		//{ "get",		_get },
		//{ "set",		_set },
		//{ "settable",	_settable },
		//{ "pairs",	_pairs },
		//{ "ipairs",	_ipairs },
		{ "init",		_init_metaTable },
		{ "share",		_share },
		{ "acquire",	_acquire },
		{ "dump",		_dump },
		{ NULL,			NULL },
	};

	luaL_checkversion(L);

	luaL_newlib(L, l);				// name, ltable

	lua_createtable(L, 0, 1);		// name, ltable, table
	lua_pushcfunction(L, _release);	// name, ltable, table, fun_r
	lua_setfield(L, -2, "__gc");	// name, ltable, table<param 1>
	lua_pushcclosure(L, _grab, 1);	// name, ltable, fun__g

	lua_setfield(L, -2, "grab");	// name, ltable

	return 1;
}
