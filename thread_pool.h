/*
 * thread_pool.h
 *
 * Declarações do padrão Pool de Threads.
 * Inclua este arquivo onde precisar usar o pool.
 *
 * Compilar junto com thread_pool.c:
 *   gcc main.c thread_pool.c -lpthread
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

/* =========================================================================
 * ESTRUTURA DE UMA TAREFA (nó da fila interna)
 * ========================================================================= */

typedef struct Task {
    void (*function)(void *arg);
    void        *arg;
    struct Task *next;
} Task;

/* =========================================================================
 * ESTRUTURA DO THREAD POOL
 * ========================================================================= */

typedef struct {
    Task *head;           /* primeiro da fila                          */
    Task *tail;           /* último da fila                            */
    int   task_count;     /* quantas tarefas estão na fila agora       */
    int   capacity;       /* capacidade máxima da fila                 */

    pthread_mutex_t lock;      /* protege todo acesso à fila           */
    pthread_cond_t  has_task;  /* acorda worker quando chega nova tarefa */
    pthread_cond_t  all_done;  /* sinaliza quando a fila esvazia        */

    pthread_t *threads;    /* array com as threads trabalhadoras       */
    int        num_threads;/* quantidade de threads no pool            */

    int shutdown;          /* 1 = pool encerrando                      */
} ThreadPool;

/* =========================================================================
 * FUNÇÕES PÚBLICAS
 * ========================================================================= */

/* Inicializa o pool com num_threads workers e fila de tamanho capacity.
 * Retorna 0 em sucesso, -1 em erro. */
int  tp_init    (ThreadPool *pool, int num_threads, int capacity);

/* Envia uma tarefa para o pool executar. */
void tp_submit  (ThreadPool *pool, void (*function)(void *), void *arg);

/* Espera todas as tarefas terminarem e encerra os workers. */
void tp_shutdown(ThreadPool *pool);

/* Libera a memória do pool. Chamar após tp_shutdown(). */
void tp_destroy (ThreadPool *pool);

#endif /* THREAD_POOL_H */