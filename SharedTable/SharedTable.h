#pragma once

#include <string>
#include <map>
#include <list>
#include <iostream>
#include <stdlib.h>
#include <atomic>
#include <mutex>

extern "C"
{
#include <lua.h>  
#include <lualib.h>  
#include <lauxlib.h>  
}

class SharedTable final
{
public:

	enum EValueType
	{
		eNil = 0,
		eString,
		eNumber,
		eBool,
		eSharedTable,
		eVoid,
		eMax,
	};

	struct SValue
	{
		~SValue() {}
		SValue() { _eType = eNil; }
		SValue(std::string val) { _eType = eString; _str = val; }
		SValue(double val) { _eType = eNumber; _d = val; }
		SValue(bool val) { _eType = eBool; _b = val; }
		SValue(SharedTable* val) { _eType = eSharedTable; _pSTable = val; }
		SValue(void* val) { _eType = eVoid; _pVoid = val; }

		bool Is(EValueType e) const { return _eType == e; }
		EValueType GetType() const { return _eType; }

		SValue(const SValue& val)
		{
			_eType = val._eType;
			switch (_eType)
			{
			default:
			case eNil:			break;
			case eString:		_str = val._str; break;
			case eNumber:		_d = val._d; break;
			case eBool:			_b = val._b; break;
			case eSharedTable:	_pSTable = val._pSTable; break;
			case eVoid:			_pVoid = val._pVoid; break;
			}
		}

		SValue& operator=(const SValue& val)
		{
			_eType = val._eType;
			switch (_eType)
			{
			default:
			case eNil:			break;
			case eString:		_str = val._str; break;
			case eNumber:		_d = val._d; break;
			case eBool:			_b = val._b; break;
			case eSharedTable:	_pSTable = val._pSTable; break;
			case eVoid:			_pVoid = val._pVoid; break;
			}

			return *this;
		}

		EValueType		_eType;
		std::string		_str;
		union
		{
			double			_d;
			bool			_b;
			SharedTable*	_pSTable;
			void*			_pVoid;
		};
	};

public:

	static int g_sharedTableCount;

	SharedTable()
	{
		_ref = 1;
		g_sharedTableCount++;
	}

	~SharedTable()
	{
		for (auto item : _childSharedTables)
		{
			item->Release();
		}
		g_sharedTableCount--;
	}

	// Note: this is not equal to lua's #
	size_t Size() const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		return _arrayContainer.size() + _mapContainer.size();
	}

	size_t ArraySize() const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		return _arrayContainer.size();
	}

	/*
	int GetArrayStartIndex() const
	{
		if (_arrayContainer.size() != 0)
			return _arrayContainer.begin()->first;

		return 0;
	}
	*/

	//------------------------------------------------------------------
	bool HasKey(int i) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		return (_arrayContainer.find(i) != _arrayContainer.end());
	}

	bool GetAt(int i, SValue& out) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		auto cit = _arrayContainer.find(i);
		if (cit != _arrayContainer.end())
		{
			out = cit->second;
			return true;
		}

		return false;
	}

	void SetAt(int i, const SValue& val)
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		_arrayContainer[i] = val;

		if (val.GetType() == eSharedTable)
			_childSharedTables.push_back(val._pSTable);
	}

	void RemoveAt(int i)
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		auto it = _arrayContainer.find(i);
		if (it != _arrayContainer.end())
		{
			if (it->second.GetType() == eSharedTable)
			{
				SharedTable* t = it->second._pSTable;
				_childSharedTables.remove(t);
				t->Release();
			}

			_arrayContainer.erase(it);
		}
	}

	bool GetArrayPair(int offset, int& key, SValue& value) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		if (offset < _arrayContainer.size())
		{
			auto itAtOffset = std::next(_arrayContainer.begin(), offset);

			key = itAtOffset->first;
			value = itAtOffset->second;
			return true;
		}

		return false;
	}


	//------------------------------------------------------------------
	bool HasKey(const std::string& key) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		return (_mapContainer.find(key) != _mapContainer.end());
	}

	bool Get(const std::string& key, SValue& out) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		auto cit = _mapContainer.find(key);
		if (cit != _mapContainer.end())
		{
			out = cit->second;
			return true;
		}

		return false;
	}

	void Set(const std::string& key, const SValue& val)
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		_mapContainer[key] = val;

		if (val.GetType() == eSharedTable)
			_childSharedTables.push_back(val._pSTable);
	}

	void Remove(const std::string& key)
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		auto it = _mapContainer.find(key);
		if (it != _mapContainer.end())
		{
			if (it->second.GetType() == eSharedTable)
			{
				SharedTable* t = it->second._pSTable;
				_childSharedTables.remove(t);
				t->Release();
			}

			_mapContainer.erase(it);
		}
	}

	bool GetMapPair(int offset, std::string& key, SValue& value) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		if (offset < _mapContainer.size())
		{
			auto itAtOffset = std::next(_mapContainer.begin(), offset);

			key = itAtOffset->first;
			value = itAtOffset->second;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------
	void DumpValue(const SValue& value, int depth, bool asLuaCode = true) const
	{
		switch (value._eType)
		{
		default:
		case eNil:
			printf("nil");
			break;
		case eString:
			printf("\"%s\"", value._str.c_str());
			break;
		case eNumber:
			printf("%g", value._d);
			break;
		case eBool:
			printf("%s", value._b ? "true" : "false");
			break;
		case eSharedTable:
			printf("{\n");
			value._pSTable->Dump(depth + 1, asLuaCode);
			for (int ident = 0; ident < depth; ++ident)
				printf("  ");
			printf("}");
			break;
		case eVoid:
			printf("0x%x", value._pVoid);
			break;
		}
	}

	void Dump(int depth, bool asLuaCode = true) const
	{
		std::lock_guard<std::recursive_mutex> guard(_mutex);

		size_t t = 0, count = Size();
		for (auto item : _arrayContainer)
		{
			for (int j = 0; j < depth; j++)
				printf("  ");

			if (!asLuaCode)
				printf("[%d] = ", item.first);

			const SValue& value = item.second;
			DumpValue(value, depth, asLuaCode);

			if (t != count - 1)
				printf(",\n");
			else
				printf("\n");

			t++;
		}

		for (auto item : _mapContainer)
		{
			for (int j = 0; j < depth; j++)
				printf("  ");

			printf("%s = ", item.first.c_str());

			const SValue& value = item.second;
			DumpValue(value, depth, asLuaCode);

			if (t != count - 1)
				printf(",\n");
			else
				printf("\n");

			t++;
		}

	}

	//------------------------------------------------------------------
	int GetRef()
	{
		std::lock_guard<std::mutex> guard(_mutexRef);

		return _ref;
	}

	void Grab()
	{
		std::lock_guard<std::mutex> guard(_mutexRef);

		_ref++;
	}

	void Release()
	{
		bool bDelete = false;
		{
			std::lock_guard<std::mutex> guard(_mutexRef);
			bDelete = (--_ref == 0);
		}

		if (bDelete)
			delete this;
	}

private:

	typedef std::map<int, SValue>			ArrayType;
	typedef std::map<std::string, SValue>	MapType;
	typedef std::list<SharedTable*>			ChildSharedTableType;

	ArrayType				_arrayContainer;
	MapType					_mapContainer;
	ChildSharedTableType	_childSharedTables;

	mutable std::recursive_mutex	_mutex;
	int								_ref;
	std::mutex						_mutexRef;
};

int luaopen_SharedTable(lua_State* L);

class SharedTableManager
{
public:

	static SharedTableManager&	GetSingleton()
	{
		static SharedTableManager g_instance;
		return g_instance;
	}

	SharedTableManager() {}
	~SharedTableManager()
	{
		// Testing code
		//SharedTable* t = GrabSharedTable("theSharedTable");
		//if (t)
		//{
		//	t->Remove("subT");
		//	t->Release();
		//}

		{
			std::lock_guard<std::mutex> guard(_mutex);
			for (auto item : _allSharedTables)
			{
				item.second->Release();
			}
		}
	
		printf("SharedTable count: %d", SharedTable::g_sharedTableCount);
	}

	bool AddSharedTable(const std::string& name, SharedTable* p)
	{
		std::lock_guard<std::mutex> guard(_mutex);

		auto it = _allSharedTables.find(name);
		if (it != _allSharedTables.end())
			return false;

		p->Grab();
		_allSharedTables[name] = p;

		return true;
	}

	// Notice: returned SharedTable's refCount will be increased.
	SharedTable* GrabSharedTable(const std::string& name) const
	{
		std::lock_guard<std::mutex> guard(_mutex);

		auto it = _allSharedTables.find(name);
		if (it != _allSharedTables.end())
		{
			it->second->Grab();
			return it->second;
		}

		return nullptr;
	}

private:
	std::map<std::string, SharedTable*>		_allSharedTables;
	mutable std::mutex						_mutex;
};

