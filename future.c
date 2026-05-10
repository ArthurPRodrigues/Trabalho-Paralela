#include "future.h"

/*
 * Inicializa o Future.
 */
void future_init(Future *f) {
    f->value = NULL;
    f->ready = 0;

    pthread_mutex_init(&f->mutex, NULL);
    pthread_cond_init(&f->cond, NULL);
}

/*
 * Define o valor do Future e acorda as threads bloqueadas.
 */
void future_set(Future *f, void *value) {
    pthread_mutex_lock(&f->mutex);

    /* Garante que o valor seja definido apenas uma vez */
    if (!f->ready) {
        f->value = value;
        f->ready = 1;

        /* Acorda todas as threads esperando */
        pthread_cond_broadcast(&f->cond);
    }

    pthread_mutex_unlock(&f->mutex);
}

/*
 * Retorna o valor.
 * Bloqueia enquanto o valor não estiver pronto.
 */
void *future_get(Future *f) {
    pthread_mutex_lock(&f->mutex);

    while (!f->ready) {
        pthread_cond_wait(&f->cond, &f->mutex);
    }

    void *result = f->value;

    pthread_mutex_unlock(&f->mutex);

    return result;
}

/*
 * Libera mutex e condition variable.
 */
void future_destroy(Future *f) {
    pthread_mutex_destroy(&f->mutex);
    pthread_cond_destroy(&f->cond);
}