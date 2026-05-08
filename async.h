/*
 * Padrão de Projeto: Async/Await (Future/Promise)
 *
 * Internamente usa o ThreadPool para executar o trabalho e uma variável
 * de condição para sincronizar o await.
 *
 * API:
 *   Future *async_run(pool, fn, arg)  → lança tarefa, retorna future
 *   void   *async_await(future)       → bloqueia até resultado ficar pronto
 *   void    future_free(future)       → libera memória
 */

#ifndef ASYNC_H
#define ASYNC_H

#include <pthread.h>
#include "thread_pool.h"

/* Estado de um Future */
typedef enum {
    FUTURE_PENDING,
    FUTURE_DONE
} FutureState;

/* Tipo da função assíncrona: recebe arg, retorna resultado (void*) */
typedef void *(*async_fn)(void *arg);

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    FutureState     state;
    void           *result;
} Future;

/*
 * Lança `fn(arg)` no pool e retorna um Future imediatamente.
 * O chamador pode continuar trabalhando e chamar async_await depois.
 */
Future *async_run(ThreadPool *pool, async_fn fn, void *arg);

/*
 * Bloqueia até o Future estar pronto e retorna o resultado.
 * Pode ser chamado apenas uma vez por Future.
 */
void *async_await(Future *f);

/* Libera o Future (chame após async_await). */
void future_free(Future *f);

#endif