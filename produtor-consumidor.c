//implementação do padrão 01 
// -> uma fila compartilhada (vai ser um array)
// -> um mutex (não permite que consumidor e produto acessem a lista ao mesmo tempo)
// -> duas variaveis de condições (para que as threads durmam quando não houver trabalho)
#include <string.h>
#include <pthread.h>
#include <stdio.h>


// tamanho da fila
#define QUEUE_SIZE 10

// implementando a fila 
typedef struct {
    void *items[QUEUE_SIZE]; 
    int start;
    int end;
    int count;
    int closed; // 1 = fila fechada, 0 = aberta

    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} BoundedQueue;

// inicializa a fila
void bq_init(BoundedQueue *queue) {
    queue->start = 0;
    queue->end = 0;
    queue->count = 0;
    queue->closed = 0;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
}

// insere item na fila
void bq_put(BoundedQueue *queue, void *item) {
    pthread_mutex_lock(&queue->lock);

    while (queue->count == QUEUE_SIZE) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    queue->items[queue->end] = item;
    queue->end = (queue->end + 1) % QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

// retira item da fila
void *bq_get(BoundedQueue *queue) {
    pthread_mutex_lock(&queue->lock);

    while (queue->count == 0 && !queue->closed) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    // fila fechada e vazia
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    void *item = queue->items[queue->start];
    queue->start = (queue->start + 1) % QUEUE_SIZE;
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);

    return item;
}

// fecha a fila e acorda todas as threads dormindo
void bq_close(BoundedQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}

// libera os recursos da fila
void bq_destroy(BoundedQueue *queue) {
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
}
