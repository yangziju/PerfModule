#include "threadPool.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <unistd.h>

#define CHAGNUM 4

void* worker(void *arg);
void* manager(void *arg);
void threadExit(ThreadPool* pool);

typedef struct Task {
    void (*handler)(void* arg);
    void* arg;
}Task;

struct ThreadPool {
    Task* taskQ;
    int qCapacity;
    int qSize;
    int qFront;
    int qBack;

    pthread_t manageID;
    pthread_t* workIDs;
    int maxNum;
    int minNum;
    int workNum;
    int liveNum;
    int exitNum;

    pthread_mutex_t mutexPool;
    pthread_mutex_t mutexWork; // 锁workNum变量
    pthread_cond_t hasTask;      // 任务队列是否有任务
    pthread_cond_t isFull;       // 任务队列是否已满

    int isDestory; // 线程池是否销毁
};

ThreadPool* threadPoolCreate(int queueSize, int minNum, int maxNum)
{
    int i, res = 0;

    // 创建线程池对象
    ThreadPool* tPool = (ThreadPool*)malloc(sizeof(struct ThreadPool));
    if (tPool == NULL) {
        perror("tPool malloc:");
        goto err;
    }

    // 创建任务队列
    tPool->taskQ = (Task*)malloc(sizeof(struct Task) * queueSize);
    if (tPool->taskQ == NULL) {
        perror("taskQ malloc:");
        goto err;
    }
    tPool->qSize = 0;
    tPool->qCapacity = queueSize;
    tPool->qFront = tPool->qBack = 0;

    // 创建存储工作线程ID的数组
    tPool->workIDs = (pthread_t*)malloc(sizeof(pthread_t) * maxNum);
    if (tPool->workIDs == NULL) {
        perror("workIDs malloc:");
        goto err;
    }
    memset(tPool->workIDs, 0, sizeof(pthread_t) * maxNum);
    tPool->maxNum = maxNum;
    tPool->minNum = minNum;
    tPool->workNum = 0;
    tPool->liveNum = minNum;
    tPool->exitNum = 0;

    tPool->isDestory = 0;
    // 初始化互斥量和条件变量
    if (pthread_mutex_init(&tPool->mutexPool, NULL) != 0 ||
        pthread_mutex_init(&tPool->mutexWork, NULL) != 0 ||
        pthread_cond_init(&tPool->isFull, NULL) != 0 ||
        pthread_cond_init(&tPool->hasTask, NULL) != 0) {
        printf("mutex or cond init fail...\n");
        goto err;
    }
    

    // 创建工作线程
    for (i = 0; i < minNum; i++) {
        res = pthread_create(&tPool->workIDs[i], NULL, worker, tPool);
        if (res != 0) {  // todo: 这里失败返回之前创建的线程可能会泄露
            printf("thread create failed for worker, errno: %d, idx: %d\n", res, i);
            goto err;
        }
    }

    // 创建管理线程
    res = pthread_create(&tPool->manageID, NULL, manager, tPool);
    if (res != 0) {
        printf("thread create failed for manager, errno: %d\n", res);
        goto err;
    }

    return tPool;

err:
    if (tPool && tPool->taskQ) {
        free(tPool->taskQ);
        tPool->taskQ = NULL;
    }

    if (tPool && tPool->workIDs) {
        free(tPool->workIDs);
        tPool->workIDs = NULL;
    }

    if (tPool) {
        free(tPool);
    }
    return NULL;
}

void* worker(void *arg)
{
    Task task;
    ThreadPool* pool = (ThreadPool*)arg;

    while(1) {
        pthread_mutex_lock(&pool->mutexPool);

        // 队列为空就阻塞当前线程，避免占用CPU
        while(pool->qSize == 0 && !pool->isDestory) {
            pthread_cond_wait(&pool->hasTask, &pool->mutexPool);
            
            // 减少空闲线程
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }

        // 销毁线程池
        if (pool->isDestory) {
            pool->liveNum--;
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        // 取一个任务执行
        task.arg = pool->taskQ[pool->qFront].arg;
        task.handler = pool->taskQ[pool->qFront].handler;
        pool->qFront = (pool->qFront + 1) % pool->qCapacity;
        pool->qSize--;
        pthread_cond_signal(&pool->isFull);
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexWork);
        pool->workNum++;
        pthread_mutex_unlock(&pool->mutexWork);
        task.handler(task.arg);
        if (task.arg) {  // 释放资源 或者 用户在回调函数中释放这里就不释放了
            free(task.arg);
            task.arg = NULL;
        }

        pthread_mutex_lock(&pool->mutexWork);
        pool->workNum--;
        pthread_mutex_unlock(&pool->mutexWork);
    }

    return NULL;
}

void* manager(void *arg)
{
    int i = 0, incNum = CHAGNUM;
    ThreadPool* pool = (ThreadPool*)arg;

    while(!pool->isDestory) {
        sleep(3);
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->qSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexWork);
        int workNum = pool->workNum;
        pthread_mutex_unlock(&pool->mutexWork);

        // 数据处理不过来要增加线程
        if (queueSize > liveNum) {
            pthread_mutex_lock(&pool->mutexPool);
            for(i = 0; i < pool->maxNum && incNum > 0; i++) {
                if (pool->workIDs[i] == 0) {
                    pthread_create(&pool->workIDs[i], NULL, worker, pool);
                    incNum--;
                    pool->liveNum++;
                    printf("new thread %ld, liveNum = %d, workNum = %d\n",
                        pool->workIDs[i], pool->liveNum, pool->workNum);
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }
        
        // 空闲线程多了要销毁
        if(workNum * 2 < liveNum &&
            liveNum - CHAGNUM > pool->minNum) {

            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = CHAGNUM;
            pthread_mutex_unlock(&pool->mutexPool);

            for (i = 0; i < CHAGNUM; i++) {
                pthread_cond_signal(&pool->hasTask);
            }
        }
    }
    return NULL;
}

int threadPoolAdd(ThreadPool* pool, void (*handler)(void* arg), void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    while(pool->qSize == pool->qCapacity && !pool->isDestory)  {
        pthread_cond_wait(&pool->isFull, &pool->mutexPool);
    }
    if (pool->isDestory) {
        pthread_mutex_unlock(&pool->mutexPool);
        return -1;
    }

    pool->taskQ[pool->qBack].arg = arg;
    pool->taskQ[pool->qBack].handler = handler;
    pool->qBack = (pool->qBack + 1) % pool->qCapacity;
    pool->qSize++;
    pthread_cond_signal(&pool->hasTask); // 通知空闲的工作线程取任务执行
    pthread_mutex_unlock(&pool->mutexPool);
    return 0;
}

void threadExit(ThreadPool* pool)
{
    int i;
    pthread_t tid = pthread_self();
    for(i = 0; i < pool->maxNum; i++) {
        if (pool->workIDs[i] == tid) {
            pool->workIDs[i] = 0;
            break;
        }
    }
    printf("thread %ld exit, liveNum = %d, workNum = %d\n",
        tid, pool->liveNum, pool->workNum);
    pthread_exit(0);
}

int threadPoolWorkNum(ThreadPool* pool)
{
    int workNum;

    pthread_mutex_lock(&pool->mutexWork);
    workNum = pool->workNum;
    pthread_mutex_unlock(&pool->mutexWork);

    return workNum;
}

int threadPoolLiveNum(ThreadPool* pool)
{
    int liveNum;

    pthread_mutex_lock(&pool->mutexPool);
    liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);

    return liveNum;
}

void threadPoolDestory(ThreadPool* pool)
{
    int i;

    if (pool == NULL) {
        return;
    }
    pool->isDestory = 1;
    // 销毁管理线程
    pthread_join(pool->manageID, NULL);

    // 销毁工作线程
    for (i = 0; i < pool->maxNum; i++) {
        if (pool->workIDs[i] > 0) {
            pthread_cond_signal(&pool->hasTask);
        }
    }
    for (i = 0; i < pool->maxNum; i++) {
        if (pool->workIDs[i] > 0) {
            pthread_join(pool->workIDs[i], NULL);
        }
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexWork);
    pthread_cond_destroy(&pool->hasTask);

    if (pool->workIDs) {
        free(pool->workIDs);
        pool->workIDs = NULL;
    }
    if (pool->taskQ) {
        free(pool->taskQ);
        pool->taskQ = NULL;
    }
    free(pool);
    printf("thread pool destory...\n");
}
