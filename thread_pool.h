/*
 * thread_pool.h
 *
 * Implementação header-only do padrão Pool de Threads em C usando pthreads.
 *
 * Como usar:
 *   #include "thread_pool.h"
 *
 * Interface pública:
 *   tp_init(&pool, num_workers, queue_cap)  → inicializa o pool
 *   tp_submit(&pool, funcao, arg)           → envia uma tarefa
 *   tp_shutdown(&pool)                      → espera tarefas acabarem e encerra
 *   tp_destroy(&pool)                       → libera memória
 *
 * Compilar:
 *   gcc seu_programa.c -lpthread
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

/* =========================================================================
 * ESTRUTURA DE UMA TAREFA
 *
 * Cada tarefa é um nó de uma lista encadeada com:
 *   - a função a executar
 *   - o argumento a passar para ela
 *   - ponteiro para a próxima tarefa na fila
 * ========================================================================= */

typedef struct Task {
    void (*function)(void *arg);  /* função a executar          */
    void        *arg;             /* argumento da função        */
    struct Task *next;            /* próxima tarefa na fila     */
} Task;

/* =========================================================================
 * ESTRUTURA DO THREAD POOL
 *
 * Contém tudo que o pool precisa para funcionar:
 *   - a fila de tarefas pendentes
 *   - o mutex que protege a fila
 *   - a variável de condição que acorda workers dormentes
 *   - o array de threads trabalhadoras
 *   - flags de controle (shutdown, drain)
 * ========================================================================= */

typedef struct {
    /* --- fila de tarefas --- */
    Task *head;           /* primeiro da fila (próximo a ser executado) */
    Task *tail;           /* último da fila (onde novas tarefas entram) */
    int   task_count;     /* quantas tarefas estão na fila agora        */
    int   capacity;       /* capacidade máxima da fila                  */

    /* --- sincronização --- */
    pthread_mutex_t lock;      /* protege todo acesso à fila            */
    pthread_cond_t  has_task;  /* acorda worker quando entra nova tarefa */
    pthread_cond_t  all_done;  /* sinaliza quando task_count chega a 0  */

    /* --- threads --- */
    pthread_t *threads;    /* array com as threads trabalhadoras        */
    int        num_threads;/* quantidade de threads no pool             */

    /* --- controle --- */
    int shutdown;  /* 1 = pool encerrando, workers devem sair do loop  */
} ThreadPool;

/* =========================================================================
 * FUNÇÃO INTERNA: loop de cada worker
 *
 * Cada thread fica neste loop para sempre até shutdown:
 *   1. Trava o mutex
 *   2. Se não há tarefas, dorme (libera o mutex enquanto dorme)
 *   3. Acorda, pega uma tarefa, destrava o mutex
 *   4. Executa a tarefa (fora do lock, para não bloquear as outras)
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
         * pthread_cond_signal acorda apenas uma thread esperando em all_done.
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
 * tp_init — inicializa o pool e cria os workers
 *
 * Parâmetros:
 *   pool        → ponteiro para o ThreadPool a inicializar
 *   num_threads → quantas threads trabalhadoras criar
 *   capacity    → tamanho máximo da fila de tarefas (0 = sem limite)
 *
 * Retorna 0 em sucesso, -1 em erro.
 * ========================================================================= */

static inline int tp_init(ThreadPool *pool, int num_threads, int capacity) {
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
 * tp_submit — envia uma tarefa para o pool executar
 *
 * Parâmetros:
 *   pool     → o pool de threads
 *   function → função a executar (assinatura: void f(void *arg))
 *   arg      → argumento a passar para a função
 * ========================================================================= */

static inline void tp_submit(ThreadPool *pool, void (*function)(void *), void *arg) {
    /* cria o nó da tarefa */
    Task *task      = malloc(sizeof(Task));
    task->function  = function;
    task->arg       = arg;
    task->next      = NULL;

    pthread_mutex_lock(&pool->lock);

    /* insere no final da fila */
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
     * Usamos signal (não broadcast) porque só chegou uma tarefa,
     * então só precisamos de um worker para pegá-la.
     */
    pthread_cond_signal(&pool->has_task);
    pthread_mutex_unlock(&pool->lock);
}

/* =========================================================================
 * tp_shutdown — espera todas as tarefas terminarem e encerra os workers
 *
 * Bloqueia até a fila esvaziar, depois sinaliza shutdown para as threads.
 * ========================================================================= */

static inline void tp_shutdown(ThreadPool *pool) {
    pthread_mutex_lock(&pool->lock);

    /* espera a fila esvaziar completamente antes de encerrar */
    while (pool->task_count > 0) {
        pthread_cond_wait(&pool->all_done, &pool->lock);
    }

    /* sinaliza shutdown para todos os workers */
    pool->shutdown = 1;

    /* acorda TODOS os workers para que verifiquem o shutdown */
    pthread_cond_broadcast(&pool->has_task);
    pthread_mutex_unlock(&pool->lock);

    /* espera cada worker terminar */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

/* =========================================================================
 * tp_destroy — libera os recursos do pool
 *
 * Deve ser chamado após tp_shutdown().
 * ========================================================================= */

static inline void tp_destroy(ThreadPool *pool) {
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy (&pool->has_task);
    pthread_cond_destroy (&pool->all_done);
    free(pool->threads);
}

#endif /* THREAD_POOL_H */