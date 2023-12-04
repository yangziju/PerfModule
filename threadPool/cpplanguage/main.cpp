#include "threadPool.h"
#include <iostream>
using namespace std;
 
void my_test(void* arg)
{
    int num = *(int*)arg;
    cout << "thread id: " << pthread_self() << " , num: " << num << endl;
    sleep(1);
    delete (int*)arg;
}

int main()
{
    ThreadPool* pool = new ThreadPool(10, 4);
    sleep(1);
    for (int i = 0; i < 30; i++) {
        Task task;
        task.handler = my_test;
        task.arg = new int(i);
        pool->addTask(task);
    }
    sleep(10);
    delete pool;
    return 0;
}