#ifndef _THREADPOOL_H
#define _THREADPOOL_H

typedef struct ThreadPool ThreadPool;

// init threadpool
ThreadPool* threadPoolcreate(int maxNum,int minNum,int queueSize);
// destory threadpool
int threadPoolDestory(ThreadPool* pool);
// add task to threadpool
void threadPoolAdd(ThreadPool* pool,void(*func)(void *),void* arg);
// get busyNum
int threadBusyNum(ThreadPool* pool);
//get liveNum
int threadAliveNum(ThreadPool* pool);
///////////////////////////////
void* worker(void* arg);
void* manager(void* arg);
void* threadExit(ThreadPool* pool);
#endif
