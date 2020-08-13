// Amitai Popovsky, 312326218 LATE-SUBMISSION

#ifndef UNTITLED6__THREADPOOL_H_
#define UNTITLED6__THREADPOOL_H_
#ifndef __THREAD_POOL__
#define __THREAD_POOL__
#include <pthread.h>
#include <stdlib.h>
#include "osqueue.h"
#include <stdio.h>
typedef struct threadPool {
  int numOfThreads;
  OSQueue *queue;
  pthread_mutex_t mutex;
  pthread_cond_t condTask;
  pthread_cond_t condRun;
  pthread_cond_t allRun;
  pthread_t *threads;
  int counterWorkingNow;
  int counterThreads;
  int shouldStop;
  int stopInsertQueue;
  int condTaskFlag;
  int condRunFlag;
  int allRunFlag;
  int mutexFlag;
  int startInsertTasks;
  int statusDestroy;
} ThreadPool;

typedef struct task {
  void (*function)(void *args);
  void *args;
} task;

ThreadPool *tpCreate(int numOfThreads);

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param);

#endif

#endif //UNTITLED6__THREADPOOL_H_
