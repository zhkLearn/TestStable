//Lua State间的跨线程数据共享
//https://blog.codingnow.com/2012/07/dev_note_24.html
/*
这个模块分了两个层次的API。其一是一组raw api，其实是直接对C函数的调用，而数据结构也是纯粹的C结构。
这样，不用Lua接口也可以访问。而Lua封装层也仅仅只是做了浅封装。尤其是不生成任何的userdata，直接
用lightuserdata保存的指针即可。当我们需要在多线程，多个LuaState间共享数据时，只需要在一个写线程上
的State中把结构创建出来，然后将指针想办法传递到另一个读线程上的State中。就可以利用这组rawapi访问
读取指针引用的C结构数据。这个读写过程是线程安全的。
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

#define ST_NIL		0
#define ST_NUMBER	1
#define ST_BOOLEAN	2
#define ST_ID		3
#define ST_STRING	4
#define ST_TABLE	5

struct STable_key
{
    int			type;
    const char*	key;
    size_t		sz_idx;
};

union UTable_value
{
    double		n;
    bool		b;
    uint64_t	id;
    void*		p;
};

struct STable;

typedef void	(*table_setstring_func)(void *ud, const char* str, size_t sz);

STable*		stable_create();
void		stable_grab(STable*);
int			stable_getref(STable*);
void		stable_release(STable*);

#define TKEY(x)		x, sizeof(x)
#define TINDEX(x)	NULL, x

// Get values
double		stable_number(STable*, const char* key, size_t sz_idx);
int			stable_boolean(STable*, const char* key, size_t sz_idx);
uint64_t	stable_id(STable*, const char* key, size_t sz_idx);
void		stable_string(STable*, const char* key, size_t sz_idx, char* buffer, size_t* bufferLen);
STable*		stable_table(STable*, const char* key, size_t sz_idx);
int			stable_type(STable*, const char* key, size_t sz_idx, UTable_value* v);
void		stable_value_string(UTable_value* v, table_setstring_func sfunc, void *ud);

// Set values
int			stable_settable(STable*, const char* key, size_t sz_idx, STable*);
int			stable_setnumber(STable*, const char* key, size_t sz_idx, double n);
int			stable_setboolean(STable*, const char* key, size_t sz_idx, bool b);
int			stable_setid(STable*, const char* key, size_t sz_idx, uint64_t id);
int			stable_setstring(STable*, const char* key, size_t sz_idx, const char* str, size_t sz);

size_t		stable_cap(STable*);
size_t		stable_keys(STable*, STable_key* v, size_t cap);

void		stable_dump(STable* root, size_t depth, bool asLuaCode);
