
#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct list_head
{
  struct list_head *next;
  struct list_head *prev;
} dlist_t;

typedef void *(*task_functor)(void *);
typedef struct thread_task {
  task_functor functor;
  void *arg;
  struct list_head list;
} thread_task_t;

typedef struct thread_pool {
  int thread_count;
  int task_num;
  struct list_head task_list;
  pthread_t *tid;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int stat;
} thread_pool_t;

thread_pool_t *threadpool_create(uint64_t thread_num);
int threadpool_add_task(thread_pool_t *pool, void *(*executor)(void *arg), void *arg);
void threadpool_destroy(thread_pool_t *pool);

#endif //_THREADPOOL_H