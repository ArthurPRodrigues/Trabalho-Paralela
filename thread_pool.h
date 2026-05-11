#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

typedef struct Task {
    void (*function)(void *arg);
    void        *arg;
    struct Task *next;
} Task;


typedef struct {
    Task *head;
    Task *tail;
    int   task_count;
    int   capacity;

    pthread_mutex_t lock;
    pthread_cond_t  has_task;
    pthread_cond_t  all_done;

    pthread_t *threads;
    int        num_threads;

    int shutdown;
} ThreadPool;

/* funções */

int  tp_init    (ThreadPool *pool, int num_threads, int capacity);

void tp_submit  (ThreadPool *pool, void (*function)(void *), void *arg);

void tp_shutdown(ThreadPool *pool);

void tp_destroy (ThreadPool *pool);

#endif