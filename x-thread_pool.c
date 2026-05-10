#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

static void *worker_loop(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;
    void *raw;
    while (bq_get(&tp->task_queue, &raw) == 0) {
        Task *t = (Task *)raw;
        t->fn(t->arg);
        free(t);
    }
    return NULL;
}

int tp_init(ThreadPool *tp, size_t num_workers, size_t queue_cap) {
    tp->num_workers = num_workers;
    tp->shutdown    = 0;
    if (bq_init(&tp->task_queue, queue_cap) != 0) return -1;

    tp->workers = malloc(num_workers * sizeof(pthread_t));
    if (!tp->workers) return -1;

    for (size_t i = 0; i < num_workers; i++) {
        if (pthread_create(&tp->workers[i], NULL, worker_loop, tp) != 0) {
            fprintf(stderr, "[ThreadPool] Falha ao criar worker %zu\n", i);
            return -1;
        }
    }
    return 0;
}

int tp_submit(ThreadPool *tp, task_fn fn, void *arg) {
    Task *t = malloc(sizeof(Task));
    if (!t) return -1;
    t->fn  = fn;
    t->arg = arg;
    return bq_put(&tp->task_queue, t);
}

void tp_shutdown(ThreadPool *tp) {
    bq_close(&tp->task_queue);
    for (size_t i = 0; i < tp->num_workers; i++)
        pthread_join(tp->workers[i], NULL);
}

void tp_destroy(ThreadPool *tp) {
    free(tp->workers);
    bq_destroy(&tp->task_queue);
}