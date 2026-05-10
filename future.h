/*
 * future.h
 *
 * Implementação genérica de Future em C usando pthreads.
 *
 * Como C não possui templates como C++, esta implementação armazena
 * um ponteiro para qualquer tipo de dado (void *).
 *
 * O Future é "one-shot":
 * - Um produtor chama future_set() uma única vez.
 * - Um ou mais consumidores chamam future_get().
 * - future_get() bloqueia até que o valor esteja disponível.
 *
 * Compilar:
 *   gcc programa.c -pthread
 */

#ifndef FUTURE_H
#define FUTURE_H

#include <pthread.h>
#include <stdlib.h>

typedef struct {
    void *value;              // ponteiro para o valor produzido
    int ready;                // 0 = não disponível, 1 = disponível
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Future;

/*
 * Inicializa o Future.
 */
static inline void future_init(Future *f) {
    f->value = NULL;
    f->ready = 0;
    pthread_mutex_init(&f->mutex, NULL);
    pthread_cond_init(&f->cond, NULL);
}

/*
 * Define o valor do Future.
 * Deve ser chamado apenas uma vez.
 */
static inline void future_set(Future *f, void *value) {
    pthread_mutex_lock(&f->mutex);

    if (!f->ready) { // verifica se ready ainda é 0
        f->value = value;
        f->ready = 1;

        // acorda todas as threads esperando
        pthread_cond_broadcast(&f->cond);
    }

    pthread_mutex_unlock(&f->mutex);
}

/*
 * Obtém o valor.
 * Bloqueia até future_set() ser chamado.
 */
static inline void *future_get(Future *f) {
    pthread_mutex_lock(&f->mutex);

    while (!f->ready) {
        pthread_cond_wait(&f->cond, &f->mutex);
    }

    void *result = f->value;

    pthread_mutex_unlock(&f->mutex);

    return result;
}

/*
 * Libera recursos internos do Future.
 * Não libera o valor armazenado.
 */
static inline void future_destroy(Future *f) {
    pthread_mutex_destroy(&f->mutex);
    pthread_cond_destroy(&f->cond);
}

#endif