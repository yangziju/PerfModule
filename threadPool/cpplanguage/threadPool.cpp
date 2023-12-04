#include "threadPool.h"
#include <iostream>

ThreadPool::ThreadPool(int max, int min)
{
    int i;
    m_maxNum = max;
    m_minNum = min;
    m_workNum = 0;
    m_liveNum = min;
    m_exitNum = 0;

    if (pthread_cond_init(&m_hasTask, nullptr) != 0
        || pthread_mutex_init(&m_lock, nullptr) != 0) {
            std::cout << "cond or mutex init fail..." << std::endl;
            return;
    }

    m_workTids = new pthread_t[m_maxNum];
    if(m_workTids == nullptr) {
        std::cout << "m_workTids malloc failed..." << std::endl;
    }
    memset(m_workTids, 0, sizeof(pthread_t) * m_maxNum);
    
    // 创建工作线程
    for (i = 0; i < m_minNum; i++) {
        pthread_create(&m_workTids[i], nullptr, worker, this); // todo: 为什么这里worker改成静态函数就没问题了？
        std::cout << "worker thread " << m_workTids[i] << " created" << std::endl;
    }

    // 创建管理线程
    pthread_create(&m_managerTid, nullptr, manager, this);
}

ThreadPool::~ThreadPool()
{
    m_isDestory = true;

    pthread_join(m_managerTid, nullptr);

    pthread_cond_broadcast(&m_hasTask);

    for (int i = 0; i < m_maxNum; i++) {
        if (m_workTids[i] != 0) {
            pthread_join(m_workTids[i], nullptr);
            std::cout << "thread i = " << i << " tid = " << m_workTids[i] << " exit..." << std::endl;
            m_workTids[i] = 0;
        }
    }

    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_hasTask);

    if (m_workTids) {
        delete []m_workTids;
    }
    std::cout << "liveNum = "<< m_liveNum <<", workNum = "<< m_workNum <<", queSize = " << this->m_taskQ.getTaskNum() << std::endl;
}

void* ThreadPool::worker(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while(1) {
        pthread_mutex_lock(&pool->m_lock);
        while(pool->m_taskQ.getTaskNum() == 0 && !pool->m_isDestory) {
            std::cout << "thread " << pthread_self() << " waitting..." << std::endl;
            pthread_cond_wait(&pool->m_hasTask, &pool->m_lock);
            // 空闲线程退出
            if (pool->m_exitNum > 0) {
                pool->m_exitNum--;
                if(pool->m_liveNum > pool->m_minNum) { // todo：为什么这里面不能直接访问成员变量，还有私有成员为什么可以用->访问？
                    pool->m_liveNum--;
                    pthread_mutex_unlock(&pool->m_lock);
                    pool->threadExit();
                }
            }
        }

        // 销毁线程池
        if (pool->m_isDestory) {
            pool->m_liveNum--;
            pthread_mutex_unlock(&pool->m_lock);
            pthread_exit(0);  // 这里不调用threadExit是让主线程好回收资源
        }
        // 取任务执行
        Task task = pool->m_taskQ.getTask();
        pool->m_workNum++;
        pthread_mutex_unlock(&pool->m_lock);
        task.handler(task.arg);  // 用户自己取释放arg内存
        pthread_mutex_lock(&pool->m_lock);
        pool->m_workNum--;
        pthread_mutex_unlock(&pool->m_lock);

    }
    return nullptr;
}

void* ThreadPool::manager(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while(!pool->m_isDestory) {
        sleep(3);

        int liveNum;
        int taskNum;
        int workNum;
        int i, incNum = pool->m_changeNum;

        pthread_mutex_lock(&pool->m_lock);
        liveNum = pool->m_liveNum;
        workNum = pool->m_workNum;
        taskNum = pool->m_taskQ.getTaskNum();
        pthread_mutex_unlock(&pool->m_lock);

        // 任务太多忙不过来需要创建线程
        if(!pool->m_isDestory && taskNum > liveNum && liveNum < pool->m_maxNum) {
            for (i = 0; i < pool->m_maxNum && incNum > 0 ; i++) {
                pthread_mutex_lock(&pool->m_lock);
                if (pool->m_workTids[i] == 0) {
                    pool->m_liveNum++;
                    incNum--;
                    pthread_create(&pool->m_workTids[i], NULL, worker, pool);
                    std::cout << "new thread " << pool->m_workTids[i] << " created" << std::endl;
                }
                pthread_mutex_unlock(&pool->m_lock);
            }
        }

        // 销毁多余的空闲线程
        incNum = pool->m_changeNum;
        if (!pool->m_isDestory && workNum * 2 < liveNum && liveNum > pool->m_minNum) {
            pthread_mutex_lock(&pool->m_lock);
            pool->m_exitNum = pool->m_changeNum;
            pthread_mutex_unlock(&pool->m_lock);
            while (incNum--) {
                pthread_cond_signal(&pool->m_hasTask);
            }
        }
    }
    return nullptr;
}

void ThreadPool::addTask(Task task)
{
    if (m_isDestory) {
        return;
    }
    pthread_mutex_lock(&m_lock);
    m_taskQ.addTask(task);
    pthread_mutex_unlock(&m_lock);
    pthread_cond_signal(&m_hasTask);
}

void ThreadPool::threadExit()
{
    for (int i = 0; i < m_maxNum; i++) {
        if (m_workTids[i] == pthread_self()) {
            std::cout << "thread " << m_workTids[i] << " exit..." << std::endl;
            pthread_mutex_lock(&m_lock);
            m_workTids[i] = 0;
            pthread_mutex_unlock(&m_lock);
            pthread_exit(0);
        }
    }
}

int ThreadPool::getWorkNum()
{
    int workNum = 0;
    pthread_mutex_lock(&m_lock);
    workNum = m_workNum;
    pthread_mutex_unlock(&m_lock);
    return m_workNum;
}

int ThreadPool::getLiveNum()
{
    int liveNum = 0;
    pthread_mutex_lock(&m_lock);
    liveNum = m_liveNum;
    pthread_mutex_unlock(&m_lock);
    return liveNum;
}

TaskQueue::TaskQueue() 
{
    pthread_mutex_init(&m_lock, NULL);
}

TaskQueue::~TaskQueue() 
{
    pthread_mutex_destroy(&m_lock);
}

void TaskQueue::addTask(Task& task)
{
    pthread_mutex_lock(&this->m_lock);
    m_que.push(task);
    pthread_mutex_unlock(&this->m_lock);
}

void TaskQueue::addTask(void (*handler)(void*), void* arg)
{
    Task task;
    task.arg = arg;
    task.handler = handler; 
    pthread_mutex_lock(&this->m_lock);
    m_que.push(task);
    pthread_mutex_unlock(&this->m_lock);
}

Task TaskQueue::getTask()
{
    Task task;

    pthread_mutex_lock(&this->m_lock);
    if (m_que.size() > 0) {
        task = m_que.front();
        m_que.pop();
    }
    pthread_mutex_unlock(&this->m_lock);

    return task;
}

inline int TaskQueue::getTaskNum()
{
    return this->m_que.size();
}