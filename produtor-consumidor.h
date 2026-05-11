#ifndef PADRAO1_ARTHUR_H
#define PADRAO1_ARTHUR_H

#include <pthread.h>

#define QUEUE_SIZE 10

typedef struct {
    void *items[QUEUE_SIZE];
    int start;
    int end;
    int count;
    int closed;

    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} BoundedQueue;

void bq_init(BoundedQueue *queue);
void bq_put(BoundedQueue *queue, void *item);
void *bq_get(BoundedQueue *queue);
void bq_close(BoundedQueue *queue);
void bq_destroy(BoundedQueue *queue);

#endif