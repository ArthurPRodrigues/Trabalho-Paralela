/*
 * Padrão de Projeto: Pool de Threads (Thread Pool)
 * Internamente usa a BoundedQueue do padrão produtor/consumidor.
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>
#include "producer_consumer.h"

/* Tipo de tarefa: função + argumento opaco */
typedef void (*task_fn)(void *arg);

typedef struct {
    task_fn  fn;
    void    *arg;
} Task;

typedef struct {
    pthread_t   *workers;
    size_t       num_workers;
    BoundedQueue task_queue;
    int          shutdown;
} ThreadPool;

int  tp_init   (ThreadPool *tp, size_t num_workers, size_t queue_cap);
int  tp_submit (ThreadPool *tp, task_fn fn, void *arg);
void tp_shutdown(ThreadPool *tp);   /* aguarda término de todas as tarefas */
void tp_destroy(ThreadPool *tp);

#endif