# Trabalho-Paralela

Arthur Paulo Rodrigues (23100747)

Adan Samuel Prüss (2410089)

Roberto Gabriel Ferreira (23100739)

# Sistema de Vendas Paralelo em C

Simulação de um fluxo de vendas concorrente utilizando três padrões
de projeto para programação paralela com Pthreads.

---

## Padrões de Projeto Utilizados

### 1. Produtor/Consumidor (`producer_consumer.h/c`)
Implementa uma **fila bloqueante thread-safe** (bounded buffer).
- **Produtores** (threads de clientes) inserem pedidos na fila.
- **Consumidor** (thread dispatcher) retira pedidos e os despacha.
- Usa `pthread_mutex_t` para exclusão mútua e **duas variáveis de
  condição** (`not_full`, `not_empty`) para bloqueio eficiente —
  diferente de busy-waiting, as threads dormem até haver espaço/item.
- A fila é **genérica** (`void*`) e pode ser reusada em qualquer projeto.

### 2. Pool de Threads (`thread_pool.h/c`)
Mantém um conjunto fixo de **N threads trabalhadoras** prontas.
- Evita o custo de criação/destruição de threads por tarefa.
- Tarefas são submetidas como par `(função, argumento)` e enfileiradas
  internamente usando o próprio `BoundedQueue`.
- Ao encerrar, `tp_shutdown` sinaliza a fila e faz `pthread_join` em
  todos os workers — garantindo que nenhum pedido seja perdido.

### 3. Future/Promise (future.h / future.c)
Este módulo implementa o padrão Future, permitindo que uma operação seja executada em segundo plano e seu resultado seja recuperado posteriormente, adiando o bloqueio da thread até o momento em que o dado é estritamente necessário.

- `future_init(f)`: Inicializa a estrutura, preparando o Mutex e a Variável de Condição necessários para a sincronização entre threads.

- `future_set(f, value)`: Define o resultado da operação. Esta função altera o estado para "pronto" e dispara um sinal (pthread_cond_broadcast) para acordar qualquer thread que esteja aguardando o valor.

- `future_get(f)`: Atua como o ponto de sincronização. Se o valor já foi definido, retorna-o imediatamente; caso contrário, bloqueia a execução da thread chamadora usando pthread_cond_wait até que future_set seja invocado.

- `future_destroy(f)`: Realiza a limpeza dos recursos de sincronização (mutex e variável de condição) alocados pelo sistema, garantindo que não haja vazamento de recursos internos.  

---

## Fluxo da Aplicação

```
Clientes (5 threads)
    │ bq_put()
    ▼
BoundedQueue (fila de pedidos)
    │ bq_get()
    ▼
Dispatcher (1 thread)
    │ tp_submit()
    ▼
ThreadPool (4 workers)
    ├─ (a) validar_cadastro()       — síncrono no worker
    ├─ (b) async_run(financeiro)    — lançado async, await posterior
    └─ (c) processar_logistica()    — síncrono no worker
```

---

## Compilação e Execução

**Pré-requisitos:** GCC com suporte a Pthreads (Linux/macOS).

```bash
# Compilar
make

# Executar
make run
# ou
./vendas

ou

# Compilar
gcc -Wall -O2 -o vendas main.c produtor-consumidor.c thread_pool.c future.c -lpthread

# Executar

./vendas
```

---

## Configurações (em `main.c`)

| Constante              | Padrão | Descrição                          |
|------------------------|--------|------------------------------------|
| `NUM_CLIENTES`         | 5      | Threads produtoras de pedidos      |
| `PEDIDOS_POR_CLIENTE`  | 4      | Pedidos por cliente (total: 20)    |
| `POOL_WORKERS`         | 4      | Workers do pool de processamento   |
| `FALHA_CADASTRO_PCT`   | 15     | % de falha em validação cadastral  |
| `FALHA_FINANCEIRO_PCT` | 20     | % de falha na operadora financeira |
| `FALHA_LOGISTICA_PCT`  | 10     | % de falha na entrega              |

---

## Saída Esperada

```
[Cliente 1] Postando pedido #001: Notebook Dell R$899.50
[Cliente 3] Postando pedido #002: SSD 1TB R$340.00
[Dispatcher] Despachando pedido #001 para o pool
[Pedido #001] Iniciando processamento | Cliente 1 | Notebook Dell | R$899.50
[Pedido #001] ✓ Cadastro ok
[Pedido #001] ↻ Aguardando retorno financeiro (async)...
[Pedido #001] ✓ Financeiro aprovado
[Pedido #001] ✓ Entrega realizada com sucesso
[Pedido #001] → STATUS FINAL: CONCLUIDO
```
