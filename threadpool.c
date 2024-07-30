#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>


const int NUMBER = 2;
// task struct
typedef struct Task
{
    void (*function)(void* arg); // pointer to the function to be executed
    void* arg; // argument to be passed to the function
}Task;
// threadpool struct
struct ThreadPool
{
    //task queue
    Task* taskQ;
    int queueCapacity; //the queue capacity
    int queueSize;     //current task number
    int queueFront;    //queue head
    int queueRear;     //queue rear

    pthread_t managerID; //manager thread ID
    pthread_t* threadIDs; // work thread IDs
    int maxNum;
    int minNum;
    int busyNum;
    int liveNum;
    int exitNum;
    pthread_mutex_t mutexPool;
    pthread_mutex_t mutexBusy;
    pthread_cond_t isFull;
    pthread_cond_t isEmpty;
    int shutdown; //destory the threadpool

};


ThreadPool *threadPoolcreate(int maxNum, int minNum, int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do
    {
    if(pool == NULL){
        printf("malloc threadpool failed...\n");
        break;
    }
    pool -> threadIDs = (pthread_t*)malloc(sizeof(pthread_t)*maxNum);
    if(pool->threadIDs == NULL){
        printf("malloc threadIDs failed...\n");
        break;
    }
    memset(pool->threadIDs,0,sizeof(pthread_t)*maxNum);
    pool -> maxNum = maxNum;
    pool -> minNum = minNum;
    pool -> busyNum = 0;
    pool -> liveNum = minNum;
    pool -> exitNum = 0;

    if(pthread_mutex_init(&pool->mutexPool, NULL)!=0||
        pthread_mutex_init(&pool->mutexBusy, NULL)!=0||
        pthread_cond_init(&pool->isFull, NULL)!=0||
        pthread_cond_init(&pool->isEmpty, NULL)!=0)
    {
        printf("init mutex or cond failed...\n");
        break;
    }

    //init task queue

    pool -> taskQ = (Task*)malloc(sizeof(Task)*queueSize);
    pool -> queueCapacity = queueSize;
    pool -> queueSize = 0;
    pool -> queueFront = 0;
    pool -> queueRear = 0;

    pool -> shutdown = 0;

    //create thread
    //manager thread
    pthread_create(&pool->managerID, NULL, manager, pool);
    //work thread (threadIDs)
    for (int i = 0; i < minNum; i++)
    {
        pthread_create(&pool->threadIDs[i], NULL, worker, pool);
    }

    return pool;
    } while (0);

    //free heap
    if(pool->taskQ) free(pool->taskQ);
    if(pool->threadIDs) free(pool->threadIDs);
    if(pool) free(pool);
    return NULL;

}

int threadPoolDestory(ThreadPool* pool)
{
    if (pool == NULL)
    {
        printf("the threadpool don't exist...\n");
        return -1;
    }
    pool->shutdown = 1;
    //block and recycle the manager thread
    pthread_join(pool->managerID,NULL);
    for (int i = 0; i < pool->liveNum; i++)
    {
        pthread_cond_signal(&pool->isEmpty);
    }
    //free heap
    if (pool->taskQ)
    {
        free(pool->taskQ);
    }
    if (pool->threadIDs)
    {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_cond_destroy(&pool->isEmpty);
    pthread_cond_destroy(&pool->isFull);

    free(pool);
    pool = NULL;
    return 0;
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void *),void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    while(pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        //block the worker thread
        pthread_cond_wait(&pool->isFull, &pool->mutexPool);
    }
    if(pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    //add task
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->isEmpty);


    pthread_mutex_unlock(&pool->mutexPool);
}

int threadBusyNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int BusyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return BusyNum;
}

int threadAliveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int AliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return AliveNum;
}

void* worker(void* arg){
    ThreadPool* pool = (ThreadPool*)arg;

    while(1){
        pthread_mutex_lock(&pool->mutexPool);
        //judge the task queue is empty or not
        while(pool->queueSize == 0 && !pool->shutdown)
        {
            //block the worker thread
            pthread_cond_wait(&pool->isEmpty, &pool->mutexPool);
            //when need to destory the thread
            if(pool->exitNum > 0)
            {
                pool->exitNum--;
                if(pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        //get task
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        //move the head node (queueFront)
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;
        pthread_cond_signal(&pool->isFull);
        //unlock
        pthread_mutex_unlock(&pool->mutexPool);
        //function
        printf("thread:%ld start working...\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;

        printf("threadID:%ld end working...\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);

    }
}

void* manager(void* arg){
    ThreadPool* pool = (ThreadPool*)arg;
    while (!pool->shutdown)
    {
        sleep(3);

        //get the number of task and the number of live thread
        //need mutex
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;            //the number of task
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);
        //get the number of busy thread
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        //add thread: add two threads at one time
        if (queueSize > liveNum - busyNum && liveNum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i++)
            {
                if (pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i],NULL, worker, pool);
                    pool->liveNum++;
                    counter++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //destory thread
        if(busyNum * 2 < liveNum && liveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);

            //destory the worker thread
            for (int i = 0; i < NUMBER; i++)
            {
                pthread_cond_signal(&pool->isEmpty);

            }

        }
    }

    return NULL;

}

void* threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    // printf("the current threadid is : %ld \n",tid);
    for (int i = 0; i < pool->maxNum; i++)
    {
        if (pool->threadIDs[i]==tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, thread:%ld exit...\n",tid);
            break;
        }

    }
    pthread_exit(NULL);

}
