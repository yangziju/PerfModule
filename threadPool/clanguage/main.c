#include "threadPool.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

void myTest(void *arg)
{
    printf("tid: %ld, num = %d\n", pthread_self(), *(int *)arg);
    sleep(3);
}

int main()
{
    int i;
    ThreadPool *pool = threadPoolCreate(20, 4, 10);
    for (i = 0; i < 40; i++) {
        int* num = (int *)malloc(sizeof(int));
        *num = i;
        threadPoolAdd(pool, myTest, num);
    }
    sleep(10);
    threadPoolDestory(pool);
    return 0;
}