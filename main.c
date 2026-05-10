/*
 * main.c - Sistema de Vendas Paralelo
 * INE5645 - Programação Paralela e Distribuída
 *
 * Membros:
 *   Arthur Paulo Rodrigues (23100747)
 *   Adan Samuel Prüss      (2410089)
 *   Roberto Gabriel Ferreira (23100739)
 *
 * Compilar: gcc -Wall -O2 -o vendas main.c produtor-consumidor.c thread_pool.c future.c -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "produtor-consumidor.h"
#include "thread_pool.h"
#include "future.h"
#include <stdarg.h>


/* ── Configurações ── */
#define NUM_CLIENTES        3
#define PEDIDOS_POR_CLIENTE 4
#define POOL_WORKERS        8
#define FALHA_CADASTRO_PCT  15
#define FALHA_FINANCEIRO_PCT 20
#define FALHA_LOGISTICA_PCT 10

/* ── Dados ── */
typedef enum { PENDENTE, CADASTRO_INVALIDO, FINANCEIRO_NEGADO, AVARIA, CONCLUIDO } Status;
static const char *status_str[] = { "PENDENTE", "CADASTRO INVALIDO", "FINANCEIRO NEGADO", "AVARIA NA ENTREGA", "CONCLUIDO" };
static const char *produtos[] = { "Notebook Dell", "Monitor LG", "Teclado Mecânico", "SSD 1TB", "RTX 4070", "RAM 32GB" };
#define NUM_PRODUTOS (int)(sizeof(produtos)/sizeof(produtos[0]))

typedef struct { int id, cliente_id; char produto[64]; double valor; Status status; } Pedido;

typedef struct { Pedido *pedido; Future *future; } FinanceiroArgs;

/* ── Globals ── */
static BoundedQueue     fila_pedidos;
static ThreadPool       pool;
static pthread_mutex_t  print_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  id_lock    = PTHREAD_MUTEX_INITIALIZER;
static int              proximo_id = 1;

/* ── Utilitários ── */
static int falha(int pct) { return (rand() % 100) < pct; }
static void log_msg(const char *fmt, ...) {
    va_list ap;
    pthread_mutex_lock(&print_lock);
    va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);
}

/* ── Etapas de processamento ── */
static void *financeiro_async(void *arg) {
    Pedido *p = (Pedido *)arg;
    usleep(80000 + rand() % 150000);
    if (falha(FALHA_FINANCEIRO_PCT)) { p->status = FINANCEIRO_NEGADO; return (void *)0; }
    return (void *)1;
}

static void financeiro_task(void *arg) {
    FinanceiroArgs *fa = (FinanceiroArgs *)arg;
    Pedido *p = fa->pedido;

    usleep(80000 + rand() % 150000);

    if (falha(FALHA_FINANCEIRO_PCT)) {
        p->status = FINANCEIRO_NEGADO;
        future_set(fa->future, (void *)0);
    } else {
        future_set(fa->future, (void *)1);
    }

    free(fa);
}

static void processar_pedido(void *arg) {
    Pedido *p = (Pedido *)arg;
    log_msg("[Pedido #%03d] Iniciando | Cliente %d | %s | R$%.2f\n", p->id, p->cliente_id, p->produto, p->valor);

    /* (a) Cadastro */
    usleep(50000 + rand() % 100000);
    if (falha(FALHA_CADASTRO_PCT)) { p->status = CADASTRO_INVALIDO; goto fim; }
    log_msg("[Pedido #%03d] ✓ Cadastro ok\n", p->id);

    /* (b) Financeiro via Future */
    Future fut;
    future_init(&fut);

    FinanceiroArgs *fa = malloc(sizeof(FinanceiroArgs));
    fa->pedido = p;
    fa->future = &fut;

    tp_submit(&pool, financeiro_task, fa);

    log_msg("[Pedido #%03d] ↻ Aguardando financeiro...\n", p->id);

    if (!(long)future_get(&fut)) {
        future_destroy(&fut);
        goto fim;
    }

    future_destroy(&fut);
    log_msg("[Pedido #%03d] ✓ Financeiro aprovado\n", p->id);

    /* (c) Logística */
    usleep(60000 + rand() % 120000);
    if (falha(FALHA_LOGISTICA_PCT)) { p->status = AVARIA; goto fim; }
    p->status = CONCLUIDO;
    log_msg("[Pedido #%03d] ✓ Entrega realizada\n", p->id);

fim:
    log_msg("[Pedido #%03d] → STATUS FINAL: %s\n\n", p->id, status_str[p->status]);
    free(p);
}

/* ── Dispatcher ── */
static void *dispatcher(void *arg) {
    (void)arg;
    void *raw;
    while ((raw = bq_get(&fila_pedidos)) != NULL)
        tp_submit(&pool, processar_pedido, raw);
    return NULL;
}

/* ── Cliente (Produtor) ── */
typedef struct { int cliente_id, num_pedidos; } ClienteArg;

static void *cliente(void *arg) {
    ClienteArg *ca = (ClienteArg *)arg;
    for (int i = 0; i < ca->num_pedidos; i++) {
        Pedido *p = malloc(sizeof(Pedido));
        pthread_mutex_lock(&id_lock); p->id = proximo_id++; pthread_mutex_unlock(&id_lock);
        p->cliente_id = ca->cliente_id;
        p->status     = PENDENTE;
        strncpy(p->produto, produtos[rand() % NUM_PRODUTOS], sizeof(p->produto) - 1);
        p->valor = 200.0 + (rand() % 10000) / 10.0;
        log_msg("[Cliente %d] Pedido #%03d: %s R$%.2f\n", p->cliente_id, p->id, p->produto, p->valor);
        bq_put(&fila_pedidos, p);
        usleep(rand() % 200000);
    }
    free(ca);
    return NULL;
}

/* ── main ── */
int main(void) {
    srand((unsigned)time(NULL));

    bq_init(&fila_pedidos);
    tp_init(&pool, POOL_WORKERS, 40);

    pthread_t disp;
    pthread_create(&disp, NULL, dispatcher, NULL);

    pthread_t clientes[NUM_CLIENTES];
    for (int i = 0; i < NUM_CLIENTES; i++) {
        ClienteArg *ca = malloc(sizeof(ClienteArg));
        ca->cliente_id = i + 1; ca->num_pedidos = PEDIDOS_POR_CLIENTE;
        pthread_create(&clientes[i], NULL, cliente, ca);
    }

    for (int i = 0; i < NUM_CLIENTES; i++) pthread_join(clientes[i], NULL);

    bq_close(&fila_pedidos);
    pthread_join(disp, NULL);

    tp_shutdown(&pool);
    tp_destroy(&pool);
    bq_destroy(&fila_pedidos);

    printf("\n=== Simulação encerrada ===\n");
    return 0;
}