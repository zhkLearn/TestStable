// SharedTable.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "SharedTable.h"

int SharedTable::g_sharedTableCount = 0;

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


static const char* _get_key(lua_State* L, int key_idx, size_t *sz_idx)
{
	int type = lua_type(L, key_idx);
	const char* key = NULL;
	size_t sz;

	switch (type)
	{
		case LUA_TNUMBER:
			sz = lua_tointeger(L, key_idx);
			key = NULL;
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

static void _set_value(lua_State* L, SharedTable* t, const char* key, size_t sz, int idx);

static bool traversal_table(lua_State* L, SharedTable* t, int idx)
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
		const char* key = _get_key(L, -2, &sz);
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

static void _set_value(lua_State* L, SharedTable* t, const char* key, size_t sz, int idx)
{
	int type = lua_type(L, idx);

#ifdef _DEBUG
	const char* typeName = nullptr;
	switch (type)
	{
		case LUA_TNIL:
			typeName = "nil";
			break;
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

	switch (type)
	{
		case LUA_TNIL:
			if (!key)
				t->SetAt(sz, SharedTable::SValue());
			else
				t->Set(key, SharedTable::SValue());
			break;

		case LUA_TNUMBER:
			if (!key)
				t->SetAt(sz, SharedTable::SValue(lua_tonumber(L, idx)));
			else
				t->Set(key, SharedTable::SValue(lua_tonumber(L, idx)));

			break;

		case LUA_TBOOLEAN:
			if (!key)
				t->SetAt(sz, SharedTable::SValue(lua_toboolean(L, idx) != 0));
			else
				t->Set(key, SharedTable::SValue(lua_toboolean(L, idx) != 0));
			break;

		case LUA_TSTRING:
			{
				size_t len;
				const char* str = lua_tolstring(L, idx, &len);
				std::string strTmp(str, len);

				if (!key)
					t->SetAt(sz, SharedTable::SValue(strTmp));
				else
					t->Set(key, SharedTable::SValue(strTmp));

				break;
			}

		case LUA_TTABLE:
			{
				SharedTable* tChild = new SharedTable();
				t->AddToChildTablesNotManagedByLua(tChild);
				if (!key)
					t->SetAt(sz, SharedTable::SValue(tChild));
				else
					t->Set(key, SharedTable::SValue(tChild));

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

		case LUA_TUSERDATA:
			{
				SharedTable* tChild = *(SharedTable**)luaL_checkudata(L, idx, "SharedTable");
				if (!key)
					t->SetAt(sz, SharedTable::SValue(tChild));
				else
					t->Set(key, SharedTable::SValue(tChild));
			}
			break;

		case LUA_TLIGHTUSERDATA:
			if (!key)
				t->SetAt(sz, SharedTable::SValue(lua_touserdata(L, idx)));
			else
				t->Set(key, SharedTable::SValue(lua_touserdata(L, idx)));

			break;

		default:
			luaL_error(L, "Unsupported value type %s", lua_typename(L, type));
	}

}

static int _set(lua_State* L)
{
	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	size_t sz;
	const char* key = _get_key(L, 2, &sz);
	_set_value(L, t, key, sz, 3);
	return 0;
}

static void _get_value(lua_State* L, const SharedTable::SValue& value)
{
	switch (value.GetType())
	{
		case SharedTable::eNil:
			lua_pushnil(L);
			break;
		case SharedTable::eString:
			lua_pushlstring(L, value._str.c_str(), value._str.length());
			break;
		case SharedTable::eNumber:
			lua_pushnumber(L, value._d);
			break;
		case SharedTable::eBool:
			lua_pushboolean(L, value._b);
			break;
		case SharedTable::eSharedTable:
			{
				SharedTable** pp = (SharedTable**)lua_newuserdata(L, sizeof(SharedTable*));
				value._pSTable->IncreaseRef();
				*pp = value._pSTable;

				luaL_getmetatable(L, "SharedTable");
				lua_setmetatable(L, -2);
			}
			break;
		case SharedTable::eVoid:
			lua_pushlightuserdata(L, value._pVoid);
			break;
	}

}

static int _get(lua_State* L)
{
	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	int type = lua_type(L, 2);

	SharedTable::SValue value;
	switch (type)
	{
		case LUA_TNUMBER:
			t->GetAt(lua_tointeger(L, 2), value);
			break;

		case LUA_TSTRING:
			{
				size_t len;
				const char* ptr = lua_tolstring(L, 2, &len);
				std::string key(ptr, len);
				t->Get(key, value);
			}
			break;

		default:
			return luaL_error(L, "Unsupported key type %s", lua_typename(L, type));
	}

	_get_value(L, value);

	return 1;
}

static int _gc(lua_State* L)
{
	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	t->DecreaseRef();

	return 0;
}

static int _new(lua_State* L)
{
	SharedTable** pp = (SharedTable**)lua_newuserdata(L, sizeof(SharedTable*));
	*pp = new SharedTable();

	luaL_getmetatable(L, "SharedTable");
	lua_setmetatable(L, -2);

	return 1;
}

static int _dump(lua_State* L)
{
	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	luaL_checkany(L, 2);
	t->Dump(0, lua_toboolean(L, 2));

	return 0;
}


static int _iter_stable_array(lua_State* L)
{
/*
	int idx = luaL_checkinteger(L, 2);
	lua_pushinteger(L, idx + 1);

	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	SharedTable::SValue value;
	if (!t->GetAt(idx + 1, value))
		return 0;

	_get_value(L, value);

	return 2;
*/

	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	size_t countArray = t->ArraySize();

	int index = lua_tointeger(L, lua_upvalueindex(1));
	if (index < countArray)
	{
		int key;
		SharedTable::SValue value;
		t->GetArrayPair(index, key, value);

		lua_pushinteger(L, key);
		_get_value(L, value);
	}
	else
	{
		lua_pushnil(L);
		lua_pushnil(L);
	}

	lua_pushinteger(L, index + 1);
	lua_replace(L, lua_upvalueindex(1));

	return 2;

}

static int _ipairs(lua_State* L)
{
	/*
	lua_pushcfunction(L, _iter_stable_array);
	lua_pushvalue(L, 1);

	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	lua_pushinteger(L, t->GetArrayStartIndex() - 1);

	return 3;
	*/

	lua_pushinteger(L, 0);
	lua_pushcclosure(L, _iter_stable_array, 1);

	lua_pushvalue(L, 1);

	return 2;

}

static int _iter_stable_all(lua_State* L)
{
	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");
	size_t count = t->Size();
	size_t countArray = t->ArraySize();

	int index = lua_tointeger(L, lua_upvalueindex(1));
	if (index < countArray)
	{
		int key;
		SharedTable::SValue value;
		t->GetArrayPair(index, key, value);

		lua_pushinteger(L, key);
		_get_value(L, value);
	}
	else if (index < count)
	{
		std::string key;
		SharedTable::SValue value;
		t->GetMapPair(index - countArray, key, value);

		lua_pushlstring(L, key.c_str(), key.length());
		_get_value(L, value);
	}
	else
	{
		lua_pushnil(L);
		lua_pushnil(L);
	}

	lua_pushinteger(L, index + 1);
	lua_replace(L, lua_upvalueindex(1));

	return 2;
}

static int _pairs(lua_State* L)
{
	lua_pushinteger(L, 0);
	lua_pushcclosure(L, _iter_stable_all, 1);

	lua_pushvalue(L, 1);

	return 2;
}

static int _share(lua_State* L)
{
	SharedTable* t = *(SharedTable**)luaL_checkudata(L, 1, "SharedTable");

	const char* name = lua_tostring(L, 2);
	if (!name)
	{
		luaL_error(L, "need a valid table name");
	}

	if (!SharedTableManager::GetSingleton().AddSharedTable(name, t))
	{
		luaL_error(L, "table already shared");
	}

	return 0;
}

static int _acquire(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	if (!name)
	{
		luaL_error(L, "need a valid table name");
	}

	SharedTable* t = SharedTableManager::GetSingleton().GrabSharedTable(name);
	if (!t)
		return 0;

	SharedTable** pp = (SharedTable**)lua_newuserdata(L, sizeof(SharedTable*));
	*pp = t;

	luaL_getmetatable(L, "SharedTable");
	lua_setmetatable(L, -2);

	return 1;
}

int luaopen_SharedTable(lua_State* L)
{
	luaL_Reg lib_methods[] =
	{
		{ "__index",	_get },
		{ "__newindex",	_set },
		{ "__gc",		_gc },
		{ "__pairs",	_pairs },
		{ "__ipairs",	_ipairs },
		{ NULL,			NULL },
	};

	luaL_Reg l[] =
	{
		{ "new",		_new },
		{ "share",		_share },
		{ "acquire",	_acquire },
		{ "dump",		_dump },
		{ NULL,			NULL },
	};

	luaL_checkversion(L);

	luaL_newmetatable(L, "SharedTable");
	luaL_setfuncs(L, lib_methods, 0);

	luaL_newlib(L, l);

	return 1;
}
