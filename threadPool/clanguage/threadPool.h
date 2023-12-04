#ifndef _THREAD_POOL_
#define _THREAD_POOL_

typedef struct ThreadPool ThreadPool;

// 创建并初始化线程池
ThreadPool* threadPoolCreate(int queueSize, int minNum, int maxNum);

// 销毁线程池
void threadPoolDestory(ThreadPool* pool);

// 往线程池添加任务
int threadPoolAdd(ThreadPool* pool, void (*handler)(void* arg), void* arg);

// 获取线程池当前工作线程数
int threadPoolWorkNum(ThreadPool* pool);

// 获取线程池当前存活线程数
int threadPoolLiveNum(ThreadPool* pool);

#endif // _THREAD_POOL_