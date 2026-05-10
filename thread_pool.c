/*
 * thread_pool.c
 *
 * Implementação do padrão Pool de Threads.
 */

#include <stdlib.h>
#include "thread_pool.h"

/* =========================================================================
 * FUNÇÃO INTERNA: loop de cada worker
 *
 * Cada thread fica neste loop até o pool encerrar:
 *   1. Trava o mutex
 *   2. Se não há tarefas, dorme (libera o mutex enquanto dorme)
 *   3. Acorda, pega uma tarefa, destrava o mutex
 *   4. Executa a tarefa fora do lock
 *   5. Volta ao passo 1
 * ========================================================================= */

static void *tp_worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        /*
         * Dorme enquanto não há tarefas e o pool ainda está ativo.
         * pthread_cond_wait libera o mutex enquanto dorme e o retoma
         * ao acordar — isso evita busy-waiting (loop sem parar).
         */
        while (pool->task_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->has_task, &pool->lock);
        }

        /* se o pool está encerrando e não há tarefas, sai do loop */
        if (pool->shutdown && pool->task_count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        /* pega a tarefa mais antiga da fila (FIFO) */
        Task *task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL) {
            pool->tail = NULL;  /* fila ficou vazia */
        }
        pool->task_count--;

        /*
         * Se a fila esvaziou, avisa tp_shutdown() que pode estar esperando.
         */
        if (pool->task_count == 0) {
            pthread_cond_signal(&pool->all_done);
        }

        pthread_mutex_unlock(&pool->lock);

        /* executa a tarefa FORA do lock para não bloquear as outras threads */
        task->function(task->arg);
        free(task);
    }

    return NULL;
}

/* =========================================================================
 * tp_init
 * ========================================================================= */

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

/* =========================================================================
 * tp_submit
 * ========================================================================= */

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

    /*
     * Acorda UM worker dormindo.
     * Usamos signal (não broadcast) porque só chegou uma tarefa.
     */
    pthread_cond_signal(&pool->has_task);
    pthread_mutex_unlock(&pool->lock);
}

/* =========================================================================
 * tp_shutdown
 * ========================================================================= */

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

/* =========================================================================
 * tp_destroy
 * ========================================================================= */

void tp_destroy(ThreadPool *pool) {
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy (&pool->has_task);
    pthread_cond_destroy (&pool->all_done);
    free(pool->threads);
}