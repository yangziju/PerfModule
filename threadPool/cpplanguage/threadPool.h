#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <queue>

struct Task  {
    void (*handler)(void*);
    void* arg = nullptr;
};

class TaskQueue {
public:
    TaskQueue();
    ~TaskQueue();

    // 添加任务
    void addTask(Task& task);
    void addTask(void (*handler)(void*), void* arg);

    // 取出任务
    Task getTask();

    // 获取任务数
    inline int getTaskNum();

private:
    pthread_mutex_t m_lock;
    std::queue<Task> m_que;
};

class ThreadPool {
public:
    ThreadPool(int max, int min);
    ~ThreadPool();

    // 添加任务
    void addTask(Task task);

    // 获取工作线程数
    int getWorkNum();

    // 获取存活线程数
    int getLiveNum();

private:
    static void* worker(void* arg);
    static void* manager(void* arg);

    void threadExit();

private:
    TaskQueue m_taskQ;
    int m_maxNum;
    int m_minNum;
    int m_workNum;
    int m_liveNum;
    int m_exitNum;
    static const int m_changeNum = 2;

    bool m_isDestory = false;

    pthread_t m_managerTid;
    pthread_t* m_workTids;

    pthread_cond_t m_hasTask;
    pthread_mutex_t m_lock; // 锁m_workNUm、m_liveNum、m_exitNum变量
};

#endif // _THREADPOOL_H