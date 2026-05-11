#include <stdlib.h>
#include "thread_pool.h"

/* --cada worker deve, até a pool acabar:
 * Travar o mutex
 * Se não tiver tarefas, dormir (libera o mutex enquanto dorme)
 * Acordar, pegar uma tarefa, destravar o mutex
 * Executar a tarefa fora do lock
 * Volta ao passo 1
 * */

static void *tp_worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        while (pool->task_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->has_task, &pool->lock);
        }

        /* sai do loop se não tiver mais tarefas */
        if (pool->shutdown && pool->task_count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        /* pega a tarefa mais antiga da fila */
        Task *task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL) {
            pool->tail = NULL;  /* fila ficou vazia */
        }
        pool->task_count--;

        /* avisa tp_shutdown() que pode estar esperando quando a fila ficar vazia.*/
        if (pool->task_count == 0) {
            pthread_cond_signal(&pool->all_done);
        }

        pthread_mutex_unlock(&pool->lock);

        /* executa a tarefa FORA do mutex_lock, se não bloqueia as outras Threads */
        task->function(task->arg);
        free(task);
    }

    return NULL;
}

int tp_init(ThreadPool *pool, int num_threads, int capacity) {
    pool->head        = NULL;
    pool->tail        = NULL;
    pool->task_count  = 0;
    pool->capacity    = capacity;
    pool->shutdown    = 0;
    pool->num_threads = num_threads;

    pthread_mutex_init(&pool->lock,     NULL);
    pthread_cond_init (&pool->has_task, NULL);
    pthread_cond_init (&pool->all_done, NULL);

    pool->threads = malloc(num_threads * sizeof(pthread_t));
    if (!pool->threads) return -1;

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, tp_worker, pool) != 0) {
            return -1;
        }
    }

    return 0;
}


void tp_submit(ThreadPool *pool, void (*function)(void *), void *arg) {
    Task *task     = malloc(sizeof(Task));
    task->function = function;
    task->arg      = arg;
    task->next     = NULL;

    pthread_mutex_lock(&pool->lock);

    if (pool->tail == NULL) {
        pool->head = task;  /* fila estava vazia */
        pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail       = task;
    }
    pool->task_count++;

    pthread_cond_signal(&pool->has_task);
    pthread_mutex_unlock(&pool->lock);
}


void tp_shutdown(ThreadPool *pool) {
    pthread_mutex_lock(&pool->lock);

    /* espera a fila esvaziar completamente */
    while (pool->task_count > 0) {
        pthread_cond_wait(&pool->all_done, &pool->lock);
    }

    pool->shutdown = 1;

    /* acorda TODOS os workers para que vejam o shutdown */
    pthread_cond_broadcast(&pool->has_task);
    pthread_mutex_unlock(&pool->lock);

    /* espera cada worker terminar */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

void tp_destroy(ThreadPool *pool) {
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy (&pool->has_task);
    pthread_cond_destroy (&pool->all_done);
    free(pool->threads);
}