#include "async.h"
#include <stdlib.h>
#include <stdio.h>

/* Wrapper interno passado ao ThreadPool */
typedef struct {
    async_fn  fn;
    void     *arg;
    Future   *future;
} AsyncWork;

static void async_worker(void *raw) {
    AsyncWork *w = (AsyncWork *)raw;
    void *result = w->fn(w->arg);

    pthread_mutex_lock(&w->future->mutex);
    w->future->result = result;
    w->future->state  = FUTURE_DONE;
    pthread_cond_signal(&w->future->cond);
    pthread_mutex_unlock(&w->future->mutex);

    free(w);
}

Future *async_run(ThreadPool *pool, async_fn fn, void *arg) {
    Future *f = malloc(sizeof(Future));
    if (!f) return NULL;

    pthread_mutex_init(&f->mutex, NULL);
    pthread_cond_init(&f->cond, NULL);
    f->state  = FUTURE_PENDING;
    f->result = NULL;

    AsyncWork *w = malloc(sizeof(AsyncWork));
    if (!w) { free(f); return NULL; }
    w->fn     = fn;
    w->arg    = arg;
    w->future = f;

    if (tp_submit(pool, async_worker, w) != 0) {
        free(w); free(f);
        return NULL;
    }
    return f;
}

void *async_await(Future *f) {
    pthread_mutex_lock(&f->mutex);
    while (f->state != FUTURE_DONE)
        pthread_cond_wait(&f->cond, &f->mutex);
    void *result = f->result;
    pthread_mutex_unlock(&f->mutex);
    return result;
}

void future_free(Future *f) {
    pthread_mutex_destroy(&f->mutex);
    pthread_cond_destroy(&f->cond);
    free(f);
}