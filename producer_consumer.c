#include "producer_consumer.h"
#include <stdlib.h>
#include <string.h>

int bq_init(BoundedQueue *q, size_t cap) {
    memset(q, 0, sizeof(*q));
    q->buffer = malloc(cap * sizeof(void *));
    if (!q->buffer) return -1;
    q->capacity = cap;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_full,  NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return 0;
}

void bq_destroy(BoundedQueue *q) {
    free(q->buffer);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

int bq_put(BoundedQueue *q, void *item) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == q->capacity && !q->closed)
        pthread_cond_wait(&q->not_full, &q->mutex);
    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    q->buffer[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int bq_get(BoundedQueue *q, void **out) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0 && !q->closed)
        pthread_cond_wait(&q->not_empty, &q->mutex);
    if (q->count == 0) {           /* fechada e vazia */
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    *out = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void bq_close(BoundedQueue *q) {
    pthread_mutex_lock(&q->mutex);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

size_t bq_size(BoundedQueue *q) {
    pthread_mutex_lock(&q->mutex);
    size_t s = q->count;
    pthread_mutex_unlock(&q->mutex);
    return s;
}