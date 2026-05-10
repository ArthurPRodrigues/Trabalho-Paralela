/*
 * Padrão de Projeto: Produtor/Consumidor (Producer-Consumer)
 */

#ifndef PRODUCER_CONSUMER_H
#define PRODUCER_CONSUMER_H

#include <pthread.h>
#include <stddef.h>

typedef struct {
    void          **buffer;
    size_t          capacity;
    size_t          head;
    size_t          tail;
    size_t          count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
    int             closed;
} BoundedQueue;

int    bq_init   (BoundedQueue *q, size_t cap);
void   bq_destroy(BoundedQueue *q);
int    bq_put    (BoundedQueue *q, void *item);   /* bloqueia se cheia  */
int    bq_get    (BoundedQueue *q, void **out);   /* bloqueia se vazia  */
void   bq_close  (BoundedQueue *q);               /* sinaliza fim       */
size_t bq_size   (BoundedQueue *q);

#endif