//implementação do padrão 01 
//para produtor-consumidor é preciso ter 3 etapas
// -> uma fila compartilhada (vai ser um array)
// -> um mutex (não permite que consumidor e produto acessem a lista ao mesmo tempo)
// -> duas variaveis de condições (para que as threads durmam quando não houver trabalho)
#include <string.h>
#include <pthread.h>
//definindo um pedido 
typedef struct {
    int id;
    int client_id;
    char product[50];
} Order;

// implementando um array
#define QUEUE_SIZE 10
// implementando a fila 

typedef struct {
    void *items[QUEUE_SIZE]; 
    int start;
    int end;
    int count;

    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;

} Queue;

//função para isnerir na fila
void queue_insert(Queue *queue, void *item) {
    pthread_mutex_lock(&queue->lock);

    while (queue->count == QUEUE_SIZE) {
        pthread_cond_wait(&queue->not_full, &queue->lock);//validar melhor isso daqui
    }

    queue->items[queue->end] = item; //coloca o order (novo pedido) como ultimo item da fila (queue->end)
    queue->end = (queue->end + 1) % QUEUE_SIZE; //fila circular ao invés de estourar
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}


void *remove_queue(Queue *queue) {
    pthread_mutex_lock(&queue->lock);

    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    void *item = queue->items[queue->start]; //salva primeiro item da fgila
    queue->start = (queue->start + 1) % QUEUE_SIZE; // avança o start circularmente
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock); // destrava mutex

    return item;
}

void *producer_thread(void *arg) {
    Queue *queue = (Queue *)arg; //informa tipo do argumento

    //produtor gera pedido
    Order order;
    order.id = 1;
    order.client_id = 123;
    strcpy(order.product, "Produto 1");

    //chama função para inserir pedido na fila
    queue_insert(queue, &order);

    return NULL;
}

void *consumer_thread(void *arg) {
    Queue *queue = (Queue *)arg;

    Order *order = (Order *)remove_queue(queue);
    printf("Processado a order #%d - produto: %s\n", order->id, order->product);

    return NULL;
}