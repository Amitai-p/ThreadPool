// Amitai Popovsky, 312326218 LATE-SUBMISSION

#include "threadPool.h"
#include <errno.h>
void exitInError(ThreadPool *threadPool, int status);
int checkThreads(ThreadPool *threadPool);
int checkWorkAndQueue(ThreadPool *threadPool);
int checkForWait(ThreadPool *threadPool);
void exitRegular(ThreadPool *threadPool);
void destroyMutexAndCond(ThreadPool *threadPool);

// Free tasks allocated.
void freeTask(task *task) {
  if (task == NULL)
    return;
  free(task);
}

// The main loop of our pool.
static void *threadPoolLoop(ThreadPool *threadPool) {
  ThreadPool *threadPoolInLoop = threadPool;
  task *task = NULL;
  while (1) {
    // Lock the mutex.
    if (pthread_mutex_lock(&threadPoolInLoop->mutex) != 0) {
      perror("Problem in lock\n");
      // Take care of the allocated.
      threadPoolInLoop->statusDestroy = -1;
      tpDestroy(threadPoolInLoop, 0);
      break;
    }
    char *msg = "Problem in pthread_cond_wait\n";
    if (threadPoolInLoop != NULL)
      if (checkForWait(threadPoolInLoop)) {
        // Wait for signal to know that there is tasks at the queue.
        if (pthread_cond_wait(&threadPoolInLoop->condTask, &threadPoolInLoop->mutex) != 0) {
          perror(msg);
          if (pthread_mutex_unlock(&threadPoolInLoop->mutex) != 0) {
            perror("Problem in unlock\n");
          }
          threadPoolInLoop->statusDestroy = -1;
          tpDestroy(threadPoolInLoop, 0);
        }
      }
    if (threadPoolInLoop != NULL)
      // To know that destroy flag.
      if (threadPoolInLoop->shouldStop) {
        break;
      }
    task = (struct task *) osDequeue(threadPoolInLoop->queue);
    // Increase the counter.
    if (threadPoolInLoop != NULL)
      threadPoolInLoop->counterWorkingNow++;
    char *msg1 = "Problem in unlock\n";
    // Unlock the mutex.
    if (threadPoolInLoop != NULL)
      if (pthread_mutex_unlock(&(threadPoolInLoop->mutex)) != 0) {
        perror(msg1);
        freeTask(task);
        threadPoolInLoop->statusDestroy = -1;
        tpDestroy(threadPoolInLoop, 0);
        break;
      }
    // Run the task.
    if (task != NULL) {
      task->function(task->args);
      // Take care of the allocated.
      freeTask(task);
    }

    char *msg2 = "Problem in lock\n";
    if (threadPoolInLoop != NULL)
      if (pthread_mutex_lock(&threadPoolInLoop->mutex) != 0) {
        perror(msg2);
        threadPoolInLoop->statusDestroy = -1;
        tpDestroy(threadPoolInLoop, 0);
        break;
      }
    // Decrease the counter.
    threadPoolInLoop->counterWorkingNow--;
    // Signal to know that all of the current tasks ended.
    if (threadPoolInLoop != NULL)
      if (checkWorkAndQueue(threadPoolInLoop)) {
        char *msg3 = "Problem in pthread_cond_signal\n";
        if (pthread_cond_signal(&threadPoolInLoop->condRun) != 0) {
          perror(msg3);
          // Take care of the allocated.
          threadPoolInLoop->statusDestroy = -1;
          tpDestroy(threadPoolInLoop, 0);
        }
      }
    char *msg4 = "Problem in unlock\n";
    if (threadPoolInLoop != NULL)
      if (pthread_mutex_unlock(&(threadPoolInLoop->mutex)) != 0) {
        perror(msg4);
        // Take care of the allocated.
        threadPoolInLoop->statusDestroy = -1;
        tpDestroy(threadPoolInLoop, 0);
      }
  }
  // Decrease the counter.
  threadPoolInLoop->counterThreads--;
  char *msg5 = "Problem in pthread_cond_signal\n";
  // Check if need to signal.
  if (checkThreads(threadPoolInLoop))
    if (pthread_cond_signal(&threadPoolInLoop->allRun) != 0) {
      perror(msg5);
      // Take care of the allocated.
      threadPoolInLoop->statusDestroy = -1;
      tpDestroy(threadPoolInLoop, 0);
    }
  char *msg6 = "Problem in unlock\n";
  if (pthread_mutex_unlock(&(threadPoolInLoop->mutex)) != 0) {
    perror(msg6);
    // Take care of the allocated.
    threadPoolInLoop->statusDestroy = -1;
    tpDestroy(threadPoolInLoop, 0);
  }
}

// Create the thread pool and return the pointer to the pool.
ThreadPool *tpCreate(int numOfThreads) {
  if (numOfThreads <= 0) {
    perror("NumOfThreads is not positive\n");
    exit(-1);
  }
  // Allocated the memory.
  ThreadPool *threadPool = (ThreadPool *) malloc(sizeof(ThreadPool));
  if (threadPool == NULL) {
    perror("Problem in malloc\n");
    exit(-1);
  }
  // Initiaillize the counter and the flags.
  threadPool->numOfThreads = 0;
  threadPool->counterWorkingNow = 0;
  threadPool->counterThreads = 0;
  threadPool->shouldStop = 0;
  threadPool->stopInsertQueue = 0;
  threadPool->statusDestroy = 0;
  threadPool->allRunFlag = 0;
  threadPool->condRunFlag = 0;
  threadPool->condTaskFlag = 0;
  threadPool->mutexFlag = 0;
  threadPool->queue = NULL;
  threadPool->threads = NULL;
  threadPool->startInsertTasks = 0;
  // Initialize the cond.
  if (pthread_cond_init(&(threadPool->condRun), NULL) != 0) {
    perror("Problem in cond_init\n");
    exitInError(threadPool, -1);
  }
  threadPool->condRunFlag = 1;
  // Initialize the cond.
  if (pthread_cond_init(&(threadPool->condTask), NULL) != 0) {
    perror("Problem in cond_init\n");
    exitInError(threadPool, -1);
  }
  threadPool->condTaskFlag = 1;
  // Initialize the cond.
  if (pthread_cond_init(&(threadPool->allRun), NULL) != 0) {
    perror("Problem in cond_init\n");
    exitInError(threadPool, -1);
  }
  threadPool->allRunFlag = 1;
  // Initialize the mutex.
  if (pthread_mutex_init(&(threadPool->mutex), NULL) != 0) {
    perror("Problem in mutex_init\n");
    exitInError(threadPool, -1);
  }
  threadPool->mutexFlag = 1;
  // Create the queue.
  threadPool->queue = osCreateQueue();
  if (threadPool->queue == NULL) {
    perror("Problem in init the queue\n");
    exitInError(threadPool, -1);
  }
  threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * (numOfThreads + 1));
  if (threadPool->threads == NULL) {
    perror("Problem in malloc\n");
    exitInError(threadPool, -1);
  }
  int i = 0;
  // Create the threads.
  pthread_attr_t pthread_attr;
  if (pthread_attr_init(&pthread_attr) != 0) {
    perror("Problem with pthread init");
    exitInError(threadPool, -1);
  }
  pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);
  int counter = 0;
  threadPool->numOfThreads = 0;
  // Set detach state.
  for (i = 0; i < numOfThreads; i++) {
    threadPool->numOfThreads = counter;
    threadPool->counterThreads = counter;
    if (pthread_create(&(threadPool->threads[i]), &pthread_attr, (void *(*)(void *)) threadPoolLoop, threadPool) != 0) {
      // In case of resource unavailable.
      if (errno == EAGAIN) {
        // In case of Resurces problem.
        perror("Problem with pthread create");
      } else {
        perror("Problem with pthread create");
      }
      threadPool->shouldStop = 1;
      exitInError(threadPool, -1);
    }

    counter++;
  }

  // Update the counter to know howw much threads created.
  threadPool->counterThreads = counter;
  threadPool->numOfThreads = threadPool->counterThreads;
  if (pthread_attr_destroy(&pthread_attr) != 0) {
    perror("Problem with pthread_attr_destroy");
    exitInError(threadPool, -1);
  }
  return threadPool;
}

// Destroy the threadPool.
void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
  // In case of another threads would like to get in destroy after we call him.
  if (threadPool == NULL) {
    perror("ThreadPool is null\n");
    exitInError(threadPool, -1);
  }
  // Check that will not be called twice.
  if (threadPool->stopInsertQueue) {
    return;
  }
  char *lock1 = "Problem in lock\n";
  // Lock the mutex.
  if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
    perror(lock1);
    exitInError(threadPool, -1);
  }
  // Flag that we was here and dont let insert more tasks.
  threadPool->stopInsertQueue = 1;
  char *unlock1 = "Problem in unlock\n";
  if (pthread_mutex_unlock(&(threadPool->mutex)) != 0) {
    perror(unlock1);
    exitInError(threadPool, -1);
  }
  // In case of that there is no tasks at the start.
  if (!threadPool->startInsertTasks|| osIsQueueEmpty(threadPool->queue)) {
    shouldWaitForTasks = 0;
  }
  if (shouldWaitForTasks == 0) {
    char *lock1 = "Problem in lock\n";
    // Lock the mutex.
    if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
      perror(lock1);
      exitInError(threadPool, -1);
    }
    // To know to stop the main loop.
    threadPool->shouldStop = 1;
    int i = 0;
    for (i = 0; i < threadPool->counterThreads; i++) {
      char *cond1 = "Problem in pthread_cond_signal\n";
      // Signal to the cond.
      if (pthread_cond_signal(&threadPool->condTask) != 0) {
        perror(cond1);
        exitInError(threadPool, -1);
      }
    }
    char *wait1 = "Problem in pthread_cond_wait\n";
    // Wait to all run cond.
    if (pthread_cond_wait(&threadPool->allRun, &threadPool->mutex) != 0) {
      perror(wait1);
      exitInError(threadPool, -1);
    }
    char *unlock2 = "Problem in unlock\n";
    // Unlock the mutex.
    if (pthread_mutex_unlock(&threadPool->mutex) != 0) {
      perror(unlock2);
      exitInError(threadPool, -1);
    }
    threadPool->statusDestroy = 0;
    // Free the memory.
    exitRegular(threadPool);
    return;
  } else {
    char *lock1 = "Problem in lock\n";
    // Lock the mutex.
    if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
      perror(lock1);
      exitInError(threadPool, -1);
    }
    // The wait error message.
    char *wait1 = "Problem in pthread_cond_wait\n";
    if (pthread_cond_wait(&threadPool->condRun, &threadPool->mutex) != 0) {
      perror(wait1);
      exitInError(threadPool, -1);
    }
    threadPool->shouldStop = 1;
    int i = 0;
    char *cond1 = "Problem in pthread_cond_signal\n";
    for (i = 0; i < threadPool->counterThreads; i++) {
      if (pthread_cond_signal(&threadPool->condTask) != 0) {
        perror(cond1);
        exitInError(threadPool, -1);
      }
    }
    char *wait2 = "Problem in pthread_cond_wait\n";
    //waiting to threads to finish.
    if (pthread_cond_wait(&threadPool->allRun, &threadPool->mutex) != 0) {
      perror(wait2);
      exitInError(threadPool, -1);
    }
    char *unlock2 = "Problem in unlock\n";
    if (pthread_mutex_unlock(&threadPool->mutex) != 0) {
      perror(unlock2);
      exitInError(threadPool, -1);
    }
    threadPool->statusDestroy = 0;
    // Free the memory.
    exitRegular(threadPool);
    return;
  }
}

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
  if (threadPool == NULL) {
    perror("ThreadPool is null\n");
    exitInError(NULL, -1);
  }
  if (threadPool->stopInsertQueue) {
    return -1;
  }
  if (computeFunc == NULL) {
    perror("The computerFunc is null\n");
    threadPool->statusDestroy = -1;
    exitInError(threadPool, -1);
  }
  // Allocated memory.
  task *t = (task *) malloc(sizeof(task));
  if (t == NULL) {
    perror("Problem in malloc\n");
    threadPool->statusDestroy = -1;
    exitInError(threadPool, -1);
  }
  t->function = computeFunc;
  t->args = param;
  // Lock the mutex.
  if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
    perror("Problem in lock\n");
    freeTask(t);
    threadPool->statusDestroy = -1;
    exitInError(threadPool, -1);
  }
  // Insert to the queue.
  osEnqueue(threadPool->queue, t);
  threadPool->startInsertTasks = 1;
  if (pthread_cond_broadcast(&(threadPool->condTask)) != 0) {
    perror("Problem in pthread_cond_broadcast\n");
    threadPool->statusDestroy = -1;
    exitInError(threadPool, -1);
  }
  // Unlock the mutex.
  if (pthread_mutex_unlock(&(threadPool->mutex)) != 0) {
    perror("Problem in unlock\n");
    threadPool->statusDestroy = -1;
    exitInError(threadPool, -1);
  }
  return 0;
}

// Free the memory.
void exitInError(ThreadPool *threadPool, int status) {
  if (threadPool == NULL) {
    exit(-1);
  }
  if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
    perror("Problem in lock\n");
    // Take care of the allocated.
  }
  threadPool->shouldStop = 1;
  threadPool->stopInsertQueue = 1;
  // Brodcast to cond.
  if (pthread_cond_broadcast(&(threadPool->condTask)) != 0) {
    perror("Problem in pthread_cond_broadcast\n");
  }
  // Brodcast to cond.
  if (pthread_cond_broadcast(&(threadPool->condRun)) != 0) {
    perror("Problem in pthread_cond_broadcast\n");
  }
  // Brodcast to cond.
  if (pthread_cond_broadcast(&(threadPool->allRun)) != 0) {
    perror("Problem in pthread_cond_broadcast\n");
  }
  int numThreads = threadPool->numOfThreads;
  int i;
  for (i = 0; i < numThreads; i++) {
    if (pthread_cond_signal(&threadPool->condTask) != 0) {
      perror("Problem with pthread_cond_signal\n");
    }
  }
  if (pthread_mutex_unlock(&(threadPool->mutex)) != 0) {
    perror("Problem in unlock\n");
    // Take care of the allocated.
  }
  for (i = 0; i < numThreads; i++) {
    if (pthread_join(threadPool->threads[i], NULL) != 0) {
      perror("Problem with pthread_join\n");
    }
  }
  // Check that is initiallized.
  if (threadPool->condTaskFlag) {
    if (pthread_cond_destroy(&(threadPool->condTask)) != 0) {
      perror("Problem in cond_destroy\n");
    }
  }
  // Check that is initiallized.
  if (threadPool->condRunFlag) {
    if (pthread_cond_destroy(&(threadPool->condRun)) != 0) {
      perror("Problem in cond_destroy\n");
    }
  }
  // Check that is initiallized.
  if (threadPool->allRunFlag) {
    if (pthread_cond_destroy(&(threadPool->allRun)) != 0) {
      perror("Problem in cond_destroy\n");
    }
  }
  // Check that is initiallized.
  if (threadPool->mutexFlag) {
    if (pthread_mutex_destroy(&(threadPool->mutex)) != 0) {
      perror("Problem in mutex_destroy\n");
    }
  }
  if (threadPool != NULL) {
    if (threadPool->queue != NULL) {
      // Free all tasks.
      while (!osIsQueueEmpty(threadPool->queue)) {
        freeTask(osDequeue(threadPool->queue));
      }
      // Free the memory.
      osDestroyQueue(threadPool->queue);
    }
    if (threadPool->threads != NULL) {
      // Free the threads array.
      free(threadPool->threads);
    }
    // Free the threadPool.
    free(threadPool);
  }
  // In case of destroy and there is no tasks.
  if (status == 2) {
    return;
  }
  exit(status);
}

// In case of destroy.
void exitRegular(ThreadPool *threadPool) {
  // Initiallize the status.
  int status = 0;
  if (threadPool == NULL) {
    exitInError(NULL, -1);
  }
  if (threadPool != NULL) {
    if (threadPool->queue != NULL) {
      // Free all tasks.
      while (!osIsQueueEmpty(threadPool->queue)) {
        freeTask(osDequeue(threadPool->queue));
      }
      // Free the memory.
      osDestroyQueue(threadPool->queue);
    }
    int i = 0;
    for (i = 0; i < threadPool->counterThreads; i++)
      if (pthread_cond_signal(&threadPool->condTask) != 0) {
        perror("error with pthread function");
      }
    for (i = 0; i < threadPool->numOfThreads; i++) {
      pthread_join(threadPool->threads[i], NULL);
    }
    if (threadPool->threads != NULL) {
      // Free the threads array.
      free(threadPool->threads);
    }
    destroyMutexAndCond(threadPool);
    status = threadPool->statusDestroy;
    // Free the threadPool.
    free(threadPool);
  }
  if (status == 0) {
    return;
  }
  exit(status);
}

// Destroy the mutex and the conds.
void destroyMutexAndCond(ThreadPool *threadPool) {
  if (pthread_cond_destroy(&(threadPool->condTask)) != 0) {
    perror("Problem in cond_destroy\n");
    // Free the memory.
    exitInError(threadPool, -1);
  }
  if (pthread_cond_destroy(&(threadPool->condRun)) != 0) {
    perror("Problem in cond_destroy\n");
    // Free the memory.
    exitInError(threadPool, -1);
  }
  if (pthread_cond_destroy(&(threadPool->allRun)) != 0) {
    perror("Problem in cond_destroy\n");
    // Free the memory.
    exitInError(threadPool, -1);
  }
  if (pthread_mutex_destroy(&(threadPool->mutex)) != 0) {
    perror("Problem in mutex_destroy\n");
    // Free the memory.
    exitInError(threadPool, -1);
  }
}

// Check flags.
int checkThreads(ThreadPool *threadPool) {
  if (threadPool->counterThreads == 0) {
    return 1;
  }
  return 0;
}

// Check flags.
int checkForWait(ThreadPool *threadPool) {
  if (!(threadPool->shouldStop) && osIsQueueEmpty(threadPool->queue)) {
    return 1;
  }
  return 0;
}

// Check flags.
int checkWorkAndQueue(ThreadPool *threadPool) {
  if (osIsQueueEmpty(threadPool->queue) && threadPool->counterWorkingNow == 0) {
    return 1;
  }
  return 0;
}

