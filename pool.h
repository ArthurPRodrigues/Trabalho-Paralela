/*
 * thread_pool.h
 *
 * Implementação de Thread Pool em C usando pthreads.
 *
 * Recursos:
 * - Número fixo de threads trabalhadoras.
 * - Fila de tarefas.
 * - Cada tarefa pode receber um argumento (void *).
 * - Integração com Future (future.h) para obter retorno assíncrono.
 *
 * Dependência:
 * - future.h (implementação enviada anteriormente)
 *
 * Compilação:
 *   gcc main.c -pthread -o programa
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "future.h"

/* ==========================================================
   Tipo da função de tarefa
   ========================================================== */
typedef void *(*TaskFunction)(void *arg);

/* ==========================================================
   Estrutura de uma tarefa
   ========================================================== */
typedef struct Task {
    TaskFunction function;
    void *arg;
    Future *future;          // pode ser NULL se não quiser retorno
    struct Task *next;
} Task;

/* ==========================================================
   Estrutura da Thread Pool
   ========================================================== */
typedef struct {
    pthread_t *threads;
    int num_threads;

    Task *head;
    Task *tail;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int stop;
} ThreadPool;

/* ==========================================================
   Worker: função executada por cada thread
   ========================================================== */
static void *thread_pool_worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        while (pool->head == NULL && !pool->stop) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if (pool->stop && pool->head == NULL) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        Task *task = pool->head;
        pool->head = task->next;

        if (pool->head == NULL) {
            pool->tail = NULL;
        }

        pthread_mutex_unlock(&pool->mutex);

        /* Executa a tarefa */
        void *result = task->function(task->arg);

        /* Se houver future, entrega o resultado */
        if (task->future != NULL) {
            future_set(task->future, result);
        }

        free(task);
    }

    return NULL;
}

/* ==========================================================
   Inicializa a Thread Pool
   ========================================================== */
static inline int thread_pool_init(ThreadPool *pool, int num_threads) {
    pool->num_threads = num_threads;
    pool->head = NULL;
    pool->tail = NULL;
    pool->stop = 0;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0)
        return -1;

    if (pthread_cond_init(&pool->cond, NULL) != 0)
        return -1;

    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL)
        return -1;

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL,
                           thread_pool_worker, pool) != 0) {
            return -1;
        }
    }

    return 0;
}

/* ==========================================================
   Submete tarefa sem retorno
   ========================================================== */
static inline int thread_pool_submit(
    ThreadPool *pool,
    TaskFunction function,
    void *arg
) {
    Task *task = malloc(sizeof(Task));
    if (task == NULL)
        return -1;

    task->function = function;
    task->arg = arg;
    task->future = NULL;
    task->next = NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->tail == NULL) {
        pool->head = task;
        pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail = task;
    }

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

/* ==========================================================
   Submete tarefa com retorno (Future)
   ========================================================== */
static inline Future *thread_pool_submit_future(
    ThreadPool *pool,
    TaskFunction function,
    void *arg
) {
    Task *task = malloc(sizeof(Task));
    if (task == NULL)
        return NULL;

    Future *future = malloc(sizeof(Future));
    if (future == NULL) {
        free(task);
        return NULL;
    }

    future_init(future);

    task->function = function;
    task->arg = arg;
    task->future = future;
    task->next = NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->tail == NULL) {
        pool->head = task;
        pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail = task;
    }

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    return future;
}

/* ==========================================================
   Finaliza a Thread Pool
   ========================================================== */
static inline void thread_pool_destroy(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutex);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);

    /* libera tarefas remanescentes */
    while (pool->head != NULL) {
        Task *tmp = pool->head;
        pool->head = pool->head->next;
        free(tmp);
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
}

#endif