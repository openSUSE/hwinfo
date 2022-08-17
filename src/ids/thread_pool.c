#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_pool.h"

#define list_entry(ptr, type, member) \
  ((type *) ((char *) (ptr) - (unsigned long) (&((type *) 0)->member)))

#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head); pos = pos->next)

#define INIT_LIST_HEAD(ptr)  (ptr)->next = (ptr)->prev = (ptr)

static inline int list_empty(const struct list_head *head)
{
  return head->next == head;
}

static inline void list_add_tail(dlist_t *elem, dlist_t *head)
{
  head->prev->next = elem;
  elem->next = head;
  elem->prev = head->prev;
  head->prev = elem;
}

static inline void list_del(dlist_t *elem)
{
  elem->next->prev = elem->prev;
  elem->prev->next = elem->next;
}


static void free_unsched_tasks(thread_pool_t *pool)
{
  thread_task_t *task = NULL;
  dlist_t *pos;

  if (pool == NULL)
    return;

  list_for_each(pos, &pool->task_list) {
    task = list_entry(pos, struct thread_task, list);
    free(task);
  }

  return;
}

static void threadpool_cancel_unlock(void *arg)
{
  thread_pool_t *pool = (thread_pool_t *)arg;
  if (pool == NULL) return;

  pthread_mutex_trylock(&pool->lock);
  pthread_mutex_unlock(&pool->lock);
}

static void *threadpool_routine(void *arg)
{
  thread_pool_t *pool = (thread_pool_t *)arg;
  task_functor functor = NULL;
  void *func_arg = NULL;

  pthread_cleanup_push(threadpool_cancel_unlock, pool);
  while (true) {
    pthread_mutex_lock(&pool->lock);
    while (__atomic_load_n(&pool->task_num, __ATOMIC_SEQ_CST) == 0) {
      pthread_cond_wait(&(pool->cond), &(pool->lock));
    }

    struct list_head *task_head = &pool->task_list;
    if (!list_empty(task_head)) {
      thread_task_t *task = list_entry(task_head->next, struct thread_task, list);
      list_del(task_head->next);
      functor = task->functor;
      func_arg = task->arg;
      free(task);
      task = NULL;
      pthread_mutex_unlock(&(pool->lock));
      (*functor)(func_arg);
      continue;
    }
    pthread_mutex_unlock(&(pool->lock));
    sleep(1);
  }
  pthread_cleanup_pop(0);

  return NULL;
}

static int pool_pthread_init(thread_pool_t *pool)
{
  if (pthread_mutex_init(&(pool->lock), NULL) != 0)
    return -1;

  if (pthread_cond_init(&(pool->cond), NULL) != 0) {
    pthread_mutex_destroy(&(pool->lock));
    return -1;
  }

  return 0;
}

thread_pool_t *threadpool_create(uint64_t thread_num)
{
  uint64_t i;

  thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
  if (pool == NULL) {
    printf("malloc for thread_pool_t fail\n");
    return NULL;
  }

  pool->tid = NULL;
  __atomic_store_n(&pool->task_num, 0, __ATOMIC_SEQ_CST);

  INIT_LIST_HEAD(&pool->task_list);

  pool->tid = (pthread_t *)malloc(thread_num * sizeof(pthread_t));
  if (pool->tid == NULL)
    goto exit_tid;

  if (pool_pthread_init(pool) != 0)
    goto exit_init;

  for (i = 0; i < thread_num; i++) {
    if (pthread_create(&(pool->tid[i]), NULL, threadpool_routine, pool) != 0) {
      printf("start threadpool fail\n");
      pool->thread_count = i;
      threadpool_destroy(pool);
      return NULL;
    }
  }

  pool->thread_count = i;
  return pool;

exit_init:
  free(pool->tid);
exit_tid:
  free(pool);

  return NULL;
}

int threadpool_add_task(thread_pool_t *pool, void *(*executor)(void *arg), void *arg)
{
  if (executor == NULL || arg == NULL) {
    printf("threadpool instance startup parameter exception.\n");
    return -1;
  }

  thread_task_t *task = (thread_task_t *)malloc(sizeof(thread_task_t));
  if (task == NULL)
    return -1;

  task->functor = executor;
  task->arg = arg;
  pthread_mutex_lock(&(pool->lock));
  list_add_tail(&task->list, &pool->task_list);
  __atomic_add_fetch(&pool->task_num, 1, __ATOMIC_SEQ_CST);
  pthread_cond_broadcast(&(pool->cond));
  pthread_mutex_unlock(&(pool->lock));

  return 0;
}

static void threadpool_cancel_thread(const thread_pool_t *pool)
{
  int i;

  for (i = 0; i < pool->thread_count; i++)
    pthread_cancel(pool->tid[i]);

  for (i = 0; i < pool->thread_count; i++)
    pthread_join(pool->tid[i], NULL);
}

static void threadpool_free(thread_pool_t *pool)
{
  if (pool == NULL)
    return;

  free(pool->tid);
  free_unsched_tasks(pool);
  pthread_mutex_destroy(&(pool->lock));
  pthread_cond_destroy(&(pool->cond));

  free(pool);
}

void threadpool_destroy(thread_pool_t *pool)
{
  if (pool == NULL)
    return;

  threadpool_cancel_thread(pool);
  threadpool_free(pool);
}

