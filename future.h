#ifndef FUTURE_H
#define FUTURE_H

#include <pthread.h>

/*
 * Estrutura que representa um Future.
 * Armazena um ponteiro genérico para o valor produzido.
 */
typedef struct {
    void *value;
    int ready;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Future;

/*
 * Inicializa o Future.
 */
void future_init(Future *f);

/*
 * Define o valor do Future.
 * Deve ser chamado apenas uma vez.
 */
void future_set(Future *f, void *value);

/*
 * Obtém o valor do Future.
 * Bloqueia até que o valor esteja disponível.
 */
void *future_get(Future *f);

/*
 * Libera os recursos internos do Future.
 * Não libera o valor armazenado.
 */
void future_destroy(Future *f);

#endif