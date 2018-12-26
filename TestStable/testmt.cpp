#include "stdafx.h"

#include "stable.h"
#include <thread>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define MAX_THREAD 16
#define MAX_COUNT 1000

static struct STable* init()
{
    struct STable * t = stable_create();
    struct STable * sub1 = stable_create();
    struct STable * sub2 = stable_create();
    stable_settable(t, TKEY("number"), sub1);
    stable_settable(t, TKEY("string"), sub2);
    stable_setnumber(t, TKEY("count"), 0);

    return t;
}

static void* thread_write(struct STable* ptr)
{
    struct STable* t = ptr;
    int i;
    char buf[32];

    struct STable * n = stable_table(t, TKEY("number"));
    struct STable * s = stable_table(t, TKEY("string"));

    assert(n && s);

    for (i = 0; i < MAX_COUNT; i++)
    {
        sprintf_s(buf, "%d", i);
		printf("Write: %6d ", i + 1);

		stable_setstring(n, TINDEX(i), buf, strlen(buf));
		printf("string ");

		stable_setnumber(s, buf, strlen(buf), i);
		printf("number ");

		stable_setnumber(t, TKEY("count"), i + 1);
		printf("count\n");

		if ((i + 1) % 10 == 0)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

    return NULL;
}

static void* thread_read(int index, struct STable* ptr)
{
    struct STable * t = ptr;
    int lastCount = 0;
    struct STable * n = stable_table(t, TKEY("number"));
    struct STable * s = stable_table(t, TKEY("string"));

    while (lastCount != MAX_COUNT)
    {
        int count = stable_number(t, TKEY("count"));

        if (count == lastCount)
        {
			continue;
		}

        if (count >= lastCount + 1)
        {
            printf("Read thread%3d: %6d - %6d\n", index, lastCount, count);
        }

        lastCount = count;
		count--;
		
		char buf[32];
		sprintf_s(buf, "%d", count);

		char buffer[512];
		size_t sz = 512;
        stable_string(n, TINDEX(count), buffer, &sz);
		int v = strtol(buffer, NULL, 10);

        //assert(v == count);
        double d = stable_number(s, buf, strlen(buf));

        if ((int)d != count)
        {
            printf("key = %s i=%d d=%f\n", buf, count, d);
        }
        //assert((int)d == count);
    }

    return NULL;
}

static void test_read(struct STable *t)
{
    struct STable * n = stable_table(t, TKEY("number"));
    struct STable * s = stable_table(t, TKEY("string"));
    char buf[32];
    int i;

    for (i = 0; i < MAX_COUNT; i++)
    {
        sprintf_s(buf, "%d", i);

		char buffer[512];
		size_t sz = 512;
		stable_string(n, TINDEX(i), buffer, &sz);
		int v = strtol(buffer, NULL, 10);
		
        //assert(v == i);
        double d = stable_number(s, buf, strlen(buf));

        if ((int)d != i)
        {
            printf("key = %s i=%d d=%f\n", buf, i, d);
        }

        //assert((int)d == i);
    }
}

int test_mt()
{
    std::thread pid[MAX_THREAD];

    struct STable *T = init();
    printf("init\n");

    pid[0] = std::thread(thread_write, T);
    //pthread_create(&pid[0], NULL, thread_write, T);

    int i;

    for (i = 1; i < MAX_THREAD; i++)
    {
        pid[i] = std::thread(thread_read, i, T);
        //pthread_create(&pid[i], NULL, thread_read, T);
    }

    for (i = 0; i < MAX_THREAD; i++)
    {
        pid[i].join();
        //pthread_join(pid[i], NULL);
    }

    printf("main exit\n");

    test_read(T);

    stable_release(T);

    return 0;
}
