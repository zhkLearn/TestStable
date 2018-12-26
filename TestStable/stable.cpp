#include "stdafx.h"

#include "stable.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include <atomic>

#define DEFAULT_SIZE 4
#define MAX_HASH_DEPTH 3
#define MAGIC_NUMBER 0x5437ab1e

#define __sync_lock_test_and_set(l, v)  std::atomic_exchange_explicit(l, v, std::memory_order_acquire)
#define __sync_lock_release(l) std::atomic_store_explicit(l, 0, std::memory_order_release);
#define __sync_add_and_fetch(l, v) (std::atomic_fetch_add(l, v) + v)
#define __sync_sub_and_fetch(l, v) (std::atomic_fetch_sub(l, v) - v)

/*
static int MEM = 0;

static void *my_malloc(size_t sz) {
	__sync_add_and_fetch (&MEM,1);
	return malloc(sz);
}
static void my_free(void *p) {
	__sync_sub_and_fetch (&MEM,1);
	free(p);
}

#define malloc my_malloc
#define free my_free
//*/

struct SMap;
struct SArray;

struct string_slot
{
    std::atomic_int ref;
    int sz;
    char buf[1];
};

struct SString
{
    std::atomic_int lock;
    struct string_slot *slot;
};

struct STable
{
    std::atomic_int ref;
    int magic;
    std::atomic_int lock;
    std::atomic_int map_lock;
    std::atomic_int array_lock;
    struct SMap *map;
    struct SArray *array;
};

struct SValue
{
    int type;
    union
    {
        double n;
        bool b;
        uint64_t id;
        STable* t;
        struct SString *s;
    } v;
};

struct SNode
{
    struct SNode *next;
    struct string_slot *k;
    SValue v;
};

struct SMap
{
    std::atomic_int ref;
    size_t size;
    struct SNode *n[1];
};

struct SArray
{
    std::atomic_int ref;
	size_t size;
    SValue a[1];
};

static inline void _table_lock(STable* t)
{
    while (__sync_lock_test_and_set(&t->lock, 1)) {}
}

static inline void _table_unlock(STable* t)
{
    __sync_lock_release(&t->lock);
}

static inline struct string_slot * _grab_string(struct SString *s)
{
    while (__sync_lock_test_and_set(&s->lock, 1)) {}

    int ref = __sync_add_and_fetch(&s->slot->ref, 1);
    assert(ref > 1);
    struct string_slot * ret = s->slot;
    __sync_lock_release(&s->lock);
    return ret;
}

static inline void _release_string(struct string_slot *s)
{
    if (__sync_sub_and_fetch(&s->ref, 1) == 0)
    {
        free(s);
    }
}

static inline struct string_slot *new_string(const char* name, size_t sz)
{
    struct string_slot *s = (struct string_slot *)malloc(sizeof(*s) + sz);
    s->ref = 1;
    s->sz = sz;
    memcpy(s->buf, name, sz);
    s->buf[sz] = '\0';
    return s;
}

static inline void _update_string(struct SString *s, const char* name, size_t sz)
{
    struct string_slot * ns = new_string(name, sz);

    while (__sync_lock_test_and_set(&s->lock, 1)) {}

    struct string_slot * old = s->slot;
    s->slot = ns;
    int ref = __sync_sub_and_fetch(&old->ref, 1);
    __sync_lock_release(&s->lock);
    if (ref == 0)
    {
        free(old);
    }
}

static inline struct SArray * _grab_array(STable* t)
{
    while (__sync_lock_test_and_set(&t->array_lock, 1)) {}

    int ref = __sync_add_and_fetch(&t->array->ref, 1);
    assert(ref > 1);
    struct SArray * ret = t->array;
    __sync_lock_release(&t->array_lock);
    return ret;
}

static inline void _release_array(struct SArray *a)
{
    if (__sync_sub_and_fetch(&a->ref, 1) == 0)
    {
        free(a);
    }
}

static inline void _update_array(STable* t, struct SArray *a)
{
    while (__sync_lock_test_and_set(&t->array_lock, 1)) {}

    struct SArray *old = t->array;
    t->array = a;
    int ref = __sync_sub_and_fetch(&old->ref, 1);
    __sync_lock_release(&t->array_lock);
    if (ref == 0)
    {
        free(old);
    }
}

static inline struct SMap* _grab_map(STable* t)
{
    while (__sync_lock_test_and_set(&t->map_lock, 1)) {}

    int ref = __sync_add_and_fetch(&t->map->ref, 1);
    assert(ref > 1);
    struct SMap * ret = t->map;
    __sync_lock_release(&t->map_lock);
    return ret;
}

static void _delete_map_without_data(struct SMap *m)
{
    for (size_t i = 0; i < m->size; i++)
    {
        struct SNode * n = m->n[i];

        while (n)
        {
            struct SNode * next = n->next;
            free(n);
            n = next;
        }
    }

    free(m);
}

static inline void _release_map(struct SMap *m)
{
    if (__sync_sub_and_fetch(&m->ref, 1) == 0)
    {
        _delete_map_without_data(m);
    }
}

static inline void _update_map(STable* t, struct SMap *m)
{
    while (__sync_lock_test_and_set(&t->map_lock, 1)) {}

    struct SMap * old = t->map;
    t->map = m;
    int ref = __sync_sub_and_fetch(&old->ref, 1);
    __sync_lock_release(&t->map_lock);
    if (ref == 0)
    {
        _delete_map_without_data(old);
    }
}

struct STable* stable_create()
{
    STable*  t = (STable* )malloc(sizeof(*t));
    memset(t, 0, sizeof(*t));
    t->ref = 1;
    t->magic = MAGIC_NUMBER;
    return t;
};

void stable_grab(STable*  t)
{
    __sync_add_and_fetch(&t->ref, 1);
}

static void _clear_value(SValue* v)
{
    switch (v->type)
    {
    case ST_STRING:
        free(v->v.s->slot);
        free(v->v.s);
        break;

    case ST_TABLE:
        stable_release(v->v.t);
        break;
    }
}

static void _delete_array(struct SArray *a)
{
    assert(a->ref == 1);
    for (size_t i = 0; i < a->size; i++)
    {
        SValue* v = &a->a[i];
        _clear_value(v);
    }

    free(a);
}

static void _delete_map(struct SMap *m)
{
    assert(m->ref == 1);
    for (size_t i = 0; i < m->size; i++)
    {
        struct SNode * n = m->n[i];

        while (n)
        {
            struct SNode * next = n->next;
            free(n->k);
            _clear_value(&n->v);
            free(n);
            n = next;
        }
    }

    free(m);
}

int stable_getref(STable* t)
{
    return t->ref;
}

void stable_release(STable* t)
{
    if (t)
    {
        if (__sync_sub_and_fetch(&t->ref, 1) != 0)
        {
            return;
        }

        if (t->array)
        {
            _delete_array(t->array);
        }

        if (t->map)
        {
            _delete_map(t->map);
        }

        t->magic = 0;
        free(t);
//		printf("memory = %d\n",MEM);
    }
}

static struct SArray* _create_array(size_t n)
{
    struct SArray *a;
    size_t sz = sizeof(*a) + (n - 1) * sizeof(SValue);
    a = (struct SArray *)malloc(sz);
    memset(a, 0, sz);
    a->ref = 1;
    a->size = n;
    return a;
}

static struct SArray* _init_array(STable* t, int cap)
{
    int size = DEFAULT_SIZE;
    while (cap >= size)
    {
        size *= 2;
    }

    struct SArray *a = _create_array(size);
    t->array = a;
    return a;
}

static struct SMap* _create_hash(size_t n)
{
    struct SMap *m;
    size_t sz = sizeof(*m) + (n - 1) * sizeof(struct SNode *);
    m = (struct SMap *)malloc(sz);
    memset(m, 0, sz);
    m->ref = 1;
    m->size = n;
    return m;
}

static struct SMap* _init_map(STable* t)
{
    struct SMap *m = _create_hash(DEFAULT_SIZE);
    t->map = m;
    return m;
}

static void _search_array(STable* t, size_t idx, SValue* result)
{
    struct SArray *a;
    do
    {
        a = _grab_array(t);

        if (idx < a->size)
        {
            *result = a->a[idx];
        }
        else
        {
            result->type = ST_NIL;
        }

        _release_array(a);
    }
    while (a != t->array);
}

static inline uint32_t hash(const char* name, size_t len)
{
    uint32_t h = (uint32_t)len;
    size_t i;

    for (i = 0; i < len; i++)
    { h = h ^ ((h << 5) + (h >> 2) + (uint32_t)name[i]); }

    return h;
}

static inline int cmp_string(struct string_slot * a, const char*  b, size_t sz)
{
    return a->sz == sz && memcmp(a->buf, b, sz) == 0;
}

static void _search_map(STable* t, const char* key, size_t sz, SValue* result)
{
    uint32_t h = hash(key, sz);
    struct SMap *m;

    do
    {
        m = _grab_map(t);
        struct SNode *n = m->n[h & (m->size - 1)];
        while (n)
        {
            if (cmp_string(n->k, key, sz))
            {
                *result = n->v;
                break;
            }
            n = n->next;
        }

        if (n == NULL)
        {
            result->type = ST_NIL;
        }
        _release_map(m);

    }
    while (m != t->map);
}

static void _search_table(STable* t, const char* key, size_t sz_idx, SValue*  result)
{
    if (key == NULL)
    {
        if (t->array)
        {
            _search_array(t, sz_idx, result);
        }
        else
        {
            result->type = ST_NIL;
        }
    }
    else
    {
        if (t->map)
        {
            _search_map(t, key, sz_idx, result);
        }
        else
        {
            result->type = ST_NIL;
        }
    }
}

int stable_type(STable* t, const char* key, size_t sz_idx, UTable_value* v)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    if (v)
    {
        memcpy(v, &tmp.v, sizeof(*v));
    }
    return tmp.type;
}

double stable_number(STable* t, const char* key, size_t sz_idx)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    assert(tmp.type == ST_NIL || tmp.type == ST_NUMBER);
    return tmp.v.n;
}

int stable_boolean(STable* t, const char* key, size_t sz_idx)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    assert(tmp.type == ST_NIL || tmp.type == ST_BOOLEAN);
    return tmp.v.b;
}

uint64_t stable_id(STable* t, const char* key, size_t sz_idx)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    assert(tmp.type == ST_NIL || tmp.type == ST_ID);
    return tmp.v.id;
}

void stable_string(STable* t, const char* key, size_t sz_idx, char* buffer, size_t* bufferLen)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    if (tmp.type == ST_STRING)
    {
        struct string_slot *s = _grab_string(tmp.v.s);
		strcpy_s(buffer, *bufferLen, s->buf);
		*bufferLen = s->sz;
        _release_string(s);
    }
    else
    {
        assert(tmp.type == ST_NIL);
		strcpy_s(buffer, *bufferLen, "");
    }
}

void stable_value_string(UTable_value * v, table_setstring_func sfunc, void * ud)
{
    struct string_slot *s = _grab_string((struct SString *)v->p);
    sfunc(ud, s->buf, s->sz);
    _release_string(s);
}

static struct SArray* _expand_array(STable* t, size_t idx)
{
    struct SArray * old = t->array;
    size_t sz = old->size;
    while (sz <= idx)
    {
        sz *= 2;
    }

    struct SArray * a = _create_array(sz);
    memcpy(a->a, old->a, old->size * sizeof(SValue));
    _update_array(t, a);

    return a;
}

static void _insert_hash(struct SMap *m, struct string_slot *k, SValue* v)
{
    uint32_t h = hash(k->buf, k->sz);
    struct SNode **pn = &m->n[h & (m->size - 1)];
    struct SNode * n = (struct SNode *)malloc(sizeof(*n));
    n->next = *pn;
    n->k = k;
    n->v = *v;
    *pn = n;
}

static void _expand_hash(STable* t)
{
    struct SMap * old = t->map;
    struct SMap * m = _create_hash(old->size * 2);
    for (size_t i = 0; i < old->size; i++)
    {
        struct SNode * n = old->n[i];
        while (n)
        {
            struct SNode * next = n->next;
            _insert_hash(m, n->k, &n->v);
            n = next;
        }
    }
    _update_map(t, m);
}

static struct SNode* _new_node(const char* key, size_t sz, int type)
{
    struct SNode * n = (struct SNode *)malloc(sizeof(*n));
    n->next = NULL;
    n->k = new_string(key, sz);
    n->v.type = type;
    return n;
}

struct STable* stable_table(STable* t, const char* key, size_t sz_idx)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    if (tmp.type == ST_TABLE)
    {
        return tmp.v.t;
    }
    assert(tmp.type == ST_NIL);
    return NULL;
}

static int _insert_array_value(STable* t, size_t idx, SValue* v)
{
    struct SArray *a = t->array;

	if (a == NULL)
    {
        a = _init_array(t, idx);
    }
    else
    {
        if (idx >= a->size)
        {
            a = _expand_array(t, idx);
        }
    }

    int type = a->a[idx].type;
    a->a[idx] = *v;
    return type;
}

static int _insert_map_value(STable* t, const char* key, size_t sz, SValue* v)
{
    struct SMap *m = t->map;
    if (m == NULL)
    {
        m = _init_map(t);
    }

    uint32_t h = hash(key, sz);
    struct SNode **pn = &m->n[h & (m->size - 1)];
    int depth = 0;
    while (*pn)
    {
        struct SNode *tmp = *pn;
        if (cmp_string(tmp->k, key, sz))
        {
            int type = tmp->v.type;
            tmp->v.v = v->v;
            return type;
        }
        pn = &tmp->next;
        ++depth;
    }

    struct SNode * n = _new_node(key, sz, v->type);
    n->v = *v;
    *pn = n;
    if (depth > MAX_HASH_DEPTH)
    {
        _expand_hash(t);
    }

    return ST_NIL;
}

static inline int _insert_table(STable* t, const char* key, size_t sz_idx, SValue* v)
{
    _table_lock(t);
    int type;
    if (key == NULL)
    {
        type = _insert_array_value(t, sz_idx, v);
    }
    else
    {
        type = _insert_map_value(t, key, sz_idx, v);
    }

    _table_unlock(t);
    return type;
}

int stable_settable(STable* t, const char* key, size_t sz_idx, STable* sub)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    if (tmp.type == ST_TABLE)
    {
        stable_release(tmp.v.t);
    }

    tmp.type = ST_TABLE;
    tmp.v.t = sub;
    int type = _insert_table(t, key, sz_idx, &tmp);
    if (type != ST_NIL && type != ST_TABLE)
    {
        return 1;
    }

    return 0;
}

int stable_setnumber(STable* t, const char* key, size_t sz_idx, double n)
{
    SValue tmp;
    tmp.type = ST_NUMBER;
    tmp.v.n = n;
    int type = _insert_table(t, key, sz_idx, &tmp);
    return type != ST_NUMBER && type != ST_NIL;
}

int stable_setboolean(STable* t, const char* key, size_t sz_idx, bool b)
{
    SValue tmp;
    tmp.type = ST_BOOLEAN;
    tmp.v.b = b;
    int type = _insert_table(t, key, sz_idx, &tmp);
    return type != ST_BOOLEAN && type != ST_NIL;
}

int stable_setid(STable* t, const char* key, size_t sz_idx, uint64_t id)
{
    SValue tmp;
    tmp.type = ST_ID;
    tmp.v.id = id;
    int type = _insert_table(t, key, sz_idx, &tmp);
    return type != ST_ID && type != ST_NIL;
}

int stable_setstring(STable* t, const char* key, size_t sz_idx, const char*  str, size_t sz)
{
    SValue tmp;
    _search_table(t, key, sz_idx, &tmp);
    if (tmp.type == ST_STRING)
    {
        _update_string(tmp.v.s, str, sz);
        return 0;
    }

    if (tmp.type != ST_NIL)
    {
        return 1;
    }

    struct SString * s = (struct SString *)malloc(sizeof(*s));
    memset(s, 0, sizeof(*s));
    s->slot = new_string(str, sz);
    tmp.type = ST_STRING;
    tmp.v.s = s;
    _insert_table(t, key, sz_idx, &tmp);

    return 0;
}

size_t stable_cap(STable* t)
{
    size_t s = 0;
    if (t->array)
    {
        struct SArray * a = _grab_array(t);
        s += a->size;
        _release_array(a);
    }

    if (t->map)
    {
        struct SMap * m = _grab_map(t);
        if (m)
        {
            s += m->size * MAX_HASH_DEPTH;
        }
        _release_map(m);
    }

    return s;
}

size_t stable_keys(STable* t, STable_key* vv, size_t cap)
{
    size_t count = 0;

    if (t->array)
    {
        struct SArray * a = _grab_array(t);
        for (size_t i = 0; i < a->size; i++)
        {
            if (count >= cap)
            {
                _release_array(a);
                return count;
            }

            SValue* v = &(a->a[i]);
            if (v->type == ST_NIL)
            {
                continue;
            }

            vv[count].type = v->type;
            vv[count].key = NULL;
            vv[count].sz_idx = i;
            ++count;
        }

        _release_array(a);
    }

    if (t->map)
    {
        struct SMap * m = _grab_map(t);
        for (size_t i = 0; i < m->size; i++)
        {
            struct SNode * n =  m->n[i];
            while (n)
            {
                if (count >= cap)
                {
                    _release_map(m);
                    return count;
                }
                vv[count].type = n->v.type;
                vv[count].key = n->k->buf;
                vv[count].sz_idx = n->k->sz;
                ++count;
                n = n->next;
            }
        }

        _release_map(m);
    }

    return count;
}

void stable_dump(STable* root, size_t depth)
{
	size_t size = stable_cap(root);
	STable_key* keys = (STable_key*)malloc(size * sizeof(*keys));

	size = stable_keys(root, keys, size);

	size_t i, j;
	for (i = 0; i < size; i++)
	{
		for (j = 0; j < depth; j++)
			printf("  ");

		if (keys[i].key == NULL)
		{
			printf("[%" PRIuPTR "] = ", keys[i].sz_idx);
		}
		else
		{
			printf("%s = ", keys[i].key);
		}

		switch (keys[i].type)
		{
		case ST_NIL:
			printf("nil");
			break;
		case ST_NUMBER:
		{
			double d = stable_number(root, keys[i].key, keys[i].sz_idx);
			printf("%lf", d);
			break;
		}
		case ST_BOOLEAN:
		{
			int b = stable_boolean(root, keys[i].key, keys[i].sz_idx);
			printf("%s", b ? "true" : "false");
			break;
		}
		case ST_ID:
		{
			uint64_t id = stable_id(root, keys[i].key, keys[i].sz_idx);
			printf("%" PRIu64, id);
			break;
		}
		case ST_STRING:
		{
			char buffer[512];
			size_t sz = 512;
			stable_string(root, keys[i].key, keys[i].sz_idx, buffer, &sz);
			printf("\"%s\"", buffer);
			break;
		}
		case ST_TABLE:
		{
			struct STable * sub = stable_table(root, keys[i].key, keys[i].sz_idx);
			printf("{\n");
			stable_dump(sub, depth + 1);
			for (int ident = 0; ident < depth; ++ident)
				printf("  ");
			printf("}");
			break;
		}
		default:
			assert(0);
			break;
		}
		printf("\n");
	}

	free(keys);
}
