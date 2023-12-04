#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* worker(void *arg)
{
    int num = *(int*)arg;

    while(1) {
        pthread_mutex_lock(&mtx);
        printf("tid: %d, will block...\n", num);
        pthread_cond_wait(&cond, &mtx);
        printf("tid: %d, start run...\n", num);
        pthread_mutex_unlock(&mtx);
        if (num == 5) {
            sleep(3);
        }
    }

    free(arg);
    return NULL;
}

int main()
{
    int i;
    int cnt;
    pthread_t tid[10];
    for (i = 0; i < 10; i++) {
        int* num = (int*)malloc(sizeof(int));
        *num = i;
        pthread_create(&tid[i], NULL, worker, num);
    }
    while(1) {
        scanf("%d", &cnt);
        pthread_mutex_lock(&mtx);
        for (i = 0; i < cnt; i++) {
            pthread_cond_signal(&cond); 
            // pthread_cond_broadcast(&cond);
            printf("send signal %d\n", i);
        }
        pthread_mutex_unlock(&mtx);
    }
    for(i = 0; i < 10; i++) {
        pthread_join(tid[i], NULL);
    }
    return 0;
}