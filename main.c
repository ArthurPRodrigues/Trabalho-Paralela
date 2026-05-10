/*
 * Fluxo:
 *  1. N threads CLIENTES produzem pedidos → BoundedQueue  (Produtor/Consumidor)
 *  2. Thread DISPATCHER consome fila e submete cada pedido ao ThreadPool
 *  3. Cada pedido passa por:
 *       a) Validação de cadastro  (síncrona no pool)
 *       b) Validação financeira   (async_run → async_await)
 *       c) Logística              (síncrona no pool)
 *  4. Status final impresso com mutex para saída ordenada.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>

#include "padrao1-arthur.h"
#include "future.h"
#include "thread_pool.h"
#include <stdint.h>

/* ================================================================== */
/*  Configurações                                                       */
/* ================================================================== */

#define NUM_CLIENTES       5      /* threads produtoras               */
#define PEDIDOS_POR_CLIENTE 4     /* pedidos que cada cliente cria    */
#define QUEUE_CAP          20     /* capacidade da fila de pedidos    */
#define POOL_WORKERS        4     /* threads do pool de processamento */
#define POOL_QUEUE_CAP     40     /* fila interna do pool             */

/* Taxas de falha (0–100) */
#define FALHA_CADASTRO_PCT  15
#define FALHA_FINANCEIRO_PCT 20
#define FALHA_LOGISTICA_PCT  10

/* ================================================================== */
/*  Modelo de dados                                                     */
/* ================================================================== */

typedef enum {
    STATUS_PENDENTE,
    STATUS_CADASTRO_INVALIDO,
    STATUS_FINANCEIRO_NEGADO,
    STATUS_AVARIA_ENTREGA,
    STATUS_CONCLUIDO
} StatusPedido;

static const char *status_str[] = {
    "PENDENTE",
    "CADASTRO INVALIDO",
    "FINANCEIRO NEGADO",
    "AVARIA NA ENTREGA",
    "CONCLUIDO"
};

typedef struct {
    int          id;
    int          cliente_id;
    char         produto[64];
    double       valor;
    StatusPedido status;
} Pedido;

/* ================================================================== */
/*  Globals                                                             */
/* ================================================================== */

static BoundedQueue  fila_pedidos;
static ThreadPool    pool_processamento;
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static int proximo_id = 1;
static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *produtos[] = {
    "Notebook Dell", "Monitor LG", "Teclado Mecânico",
    "SSD 1TB", "Placa de Vídeo RTX4070", "Memória RAM 32GB"
};
#define NUM_PRODUTOS (int)(sizeof(produtos)/sizeof(produtos[0]))

/* ================================================================== */
/*  Utilitários                                                         */
/* ================================================================== */

static int falha_aleatoria(int pct) {
    return (rand() % 100) < pct;
}

static void log_msg(const char *fmt, ...) {
    va_list ap;
    pthread_mutex_lock(&print_mutex);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

/* ================================================================== */
/*  Etapas de processamento                                             */
/* ================================================================== */

/* (a) Validação de Cadastro — retorna 1 se ok */
static int validar_cadastro(Pedido *p) {
    usleep(50000 + rand() % 100000);   /* simula latência 50–150ms */
    if (falha_aleatoria(FALHA_CADASTRO_PCT)) {
        p->status = STATUS_CADASTRO_INVALIDO;
        return 0;
    }
    return 1;
}

/* (b) Validação Financeira — chamada como async_fn */
static void *validar_financeiro_async(void *arg) {
    Pedido *p = (Pedido *)arg;
    usleep(80000 + rand() % 150000);   /* simula resposta da operadora */
    if (falha_aleatoria(FALHA_FINANCEIRO_PCT)) {
        p->status = STATUS_FINANCEIRO_NEGADO;
        return (void *)0;
    }
    return (void *)1;
}

/* (c) Logística — retorna 1 se ok */
static int processar_logistica(Pedido *p) {
    usleep(60000 + rand() % 120000);
    if (falha_aleatoria(FALHA_LOGISTICA_PCT)) {
        p->status = STATUS_AVARIA_ENTREGA;
        return 0;
    }
    p->status = STATUS_CONCLUIDO;
    return 1;
}

/* ================================================================== */
/*  Tarefa do pool: processa um pedido completo                         */
/* ================================================================== */

static void processar_pedido(void *arg) {
    Pedido *p = (Pedido *)arg;

    log_msg("[Pedido #%03d] Iniciando processamento | Cliente %d | %s | R$%.2f\n",
            p->id, p->cliente_id, p->produto, p->valor);

    /* (a) Validação de cadastro */
    if (!validar_cadastro(p)) {
        log_msg("[Pedido #%03d] ✗ CADASTRO INVALIDO\n", p->id);
        goto finalizar;
    }
    log_msg("[Pedido #%03d] ✓ Cadastro ok\n", p->id);

    /* (b) Validação financeira — async: lança e segue */
    Future *fut = async_run(&pool_processamento, validar_financeiro_async, p);
    if (!fut) {
        log_msg("[Pedido #%03d] Erro ao lançar validação financeira\n", p->id);
        p->status = STATUS_FINANCEIRO_NEGADO;
        goto finalizar;
    }

    /* Aqui poderíamos fazer outras operações enquanto aguardamos... */
    log_msg("[Pedido #%03d] ↻ Aguardando retorno financeiro (async)...\n", p->id);

    intptr_t ok = (intptr_t)async_await(fut);
    future_free(fut);

    if (!ok) {
        log_msg("[Pedido #%03d] ✗ OPERACAO FINANCEIRA NEGADA\n", p->id);
        goto finalizar;
    }
    log_msg("[Pedido #%03d] ✓ Financeiro aprovado\n", p->id);

    /* (c) Logística */
    if (!processar_logistica(p)) {
        log_msg("[Pedido #%03d] ✗ AVARIA NA ENTREGA\n", p->id);
        goto finalizar;
    }
    log_msg("[Pedido #%03d] ✓ Entrega realizada com sucesso\n", p->id);

finalizar:
    log_msg("[Pedido #%03d] → STATUS FINAL: %s\n\n", p->id, status_str[p->status]);
    free(p);
}

/* ================================================================== */
/*  Thread Dispatcher: consome fila e envia ao pool                    */
/* ================================================================== */

static void *dispatcher(void *arg) {
    (void)arg;
    void *raw;
    while (bq_get(&fila_pedidos, &raw) == 0) {
        Pedido *p = (Pedido *)raw;
        log_msg("[Dispatcher] Despachando pedido #%03d para o pool\n", p->id);
        tp_submit(&pool_processamento, processar_pedido, p);
    }
    log_msg("[Dispatcher] Fila encerrada. Dispatcher finalizado.\n");
    return NULL;
}

/* ================================================================== */
/*  Thread Cliente (Produtor)                                           */
/* ================================================================== */

typedef struct { int cliente_id; int num_pedidos; } ClienteArg;

static void *cliente(void *arg) {
    ClienteArg *ca = (ClienteArg *)arg;

    for (int i = 0; i < ca->num_pedidos; i++) {
        Pedido *p = malloc(sizeof(Pedido));
        if (!p) continue;

        pthread_mutex_lock(&id_mutex);
        p->id = proximo_id++;
        pthread_mutex_unlock(&id_mutex);

        p->cliente_id = ca->cliente_id;
        p->status     = STATUS_PENDENTE;
        strncpy(p->produto, produtos[rand() % NUM_PRODUTOS], sizeof(p->produto)-1);
        p->valor = 200.0 + (rand() % 10000) / 10.0;

        log_msg("[Cliente %d] Postando pedido #%03d: %s R$%.2f\n",
                ca->cliente_id, p->id, p->produto, p->valor);

        bq_put(&fila_pedidos, p);
        usleep(rand() % 200000);   /* intervalo aleatório entre pedidos */
    }

    log_msg("[Cliente %d] Todos os pedidos enviados.\n", ca->cliente_id);
    free(ca);
    return NULL;
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */

int main(void) {
    srand((unsigned)time(NULL));

    printf("==========================================================\n");
    printf("  Sistema de Vendas Paralelo — Padrões de Projeto em C\n");
    printf("==========================================================\n");
    printf("  Clientes       : %d  (cada um gera %d pedidos)\n",
           NUM_CLIENTES, PEDIDOS_POR_CLIENTE);
    printf("  Pool de threads: %d workers\n", POOL_WORKERS);
    printf("  Taxas de falha : cadastro=%d%%, financeiro=%d%%, logística=%d%%\n",
           FALHA_CADASTRO_PCT, FALHA_FINANCEIRO_PCT, FALHA_LOGISTICA_PCT);
    printf("==========================================================\n\n");

    /* 1. Inicializar fila de pedidos (Produtor/Consumidor) */
    if (bq_init(&fila_pedidos, QUEUE_CAP) != 0) {
        fprintf(stderr, "Erro ao inicializar fila de pedidos\n");
        return 1;
    }

    /* 2. Inicializar pool de threads */
    if (tp_init(&pool_processamento, POOL_WORKERS, POOL_QUEUE_CAP) != 0) {
        fprintf(stderr, "Erro ao inicializar thread pool\n");
        return 1;
    }

    /* 3. Criar thread dispatcher */
    pthread_t disp_thread;
    pthread_create(&disp_thread, NULL, dispatcher, NULL);

    /* 4. Criar threads clientes (produtores) */
    pthread_t cliente_threads[NUM_CLIENTES];
    for (int i = 0; i < NUM_CLIENTES; i++) {
        ClienteArg *ca = malloc(sizeof(ClienteArg));
        ca->cliente_id  = i + 1;
        ca->num_pedidos = PEDIDOS_POR_CLIENTE;
        pthread_create(&cliente_threads[i], NULL, cliente, ca);
    }

    /* 5. Aguardar todos os clientes terminarem de produzir */
    for (int i = 0; i < NUM_CLIENTES; i++)
        pthread_join(cliente_threads[i], NULL);

    /* 6. Fechar fila → dispatcher e pool encerram ao esvaziar */
    printf("\n[Main] Todos os clientes finalizaram. Fechando fila...\n\n");
    bq_close(&fila_pedidos);

    /* 7. Aguardar dispatcher */
    pthread_join(disp_thread, NULL);

    /* 8. Encerrar pool (aguarda tarefas pendentes) */
    tp_shutdown(&pool_processamento);

    /* 9. Cleanup */
    tp_destroy(&pool_processamento);
    bq_destroy(&fila_pedidos);

    printf("\n==========================================================\n");
    printf("  Simulação encerrada. Todos os pedidos foram processados.\n");
    printf("==========================================================\n");
    return 0;
}