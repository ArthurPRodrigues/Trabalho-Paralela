# Observação importante: risco de deadlock

Há um detalhe sério no desenho atual.

`processar_pedido()` já está sendo executado dentro do próprio `ThreadPool`.

Dentro dela, você submete outra tarefa ao mesmo pool e fica esperando.

Se todos os workers estiverem ocupados em `future_get()`, ninguém ficará livre para executar `financeiro_task()`. Isso gera deadlock.

---

# Exemplo do deadlock

Suponha:

* `POOL_WORKERS = 4`

Os 4 workers fazem:

1. Executam `processar_pedido()`.
2. Cada um submete `financeiro_task()`.
3. Cada um bloqueia em `future_get()`.

Agora:

* 4 workers estão bloqueados.
* 4 tarefas financeiras estão na fila.
* Nenhum worker está livre.

O sistema trava.

---

# Soluções

## Solução 1 (mais simples)

Aumentar o número de workers para um valor bem maior.

Exemplo:

```c
#define POOL_WORKERS 8
```

Funciona, mas não elimina o problema estrutural.

---

## Solução 2 (melhor)

Usar um pool separado para o financeiro.

```c
static ThreadPool financeiro_pool;
```

Assim:

* `pool` → processamento geral
* `financeiro_pool` → validação financeira

Essa é a solução mais correta.

---

# Minha recomendação

Para o trabalho acadêmico, a melhor abordagem é usar **dois thread pools**:

* `pool_processamento`
* `pool_financeiro`

Isso demonstra maturidade de projeto e evita deadlocks.

---

# Resumo

O travamento acontece porque:

1. O `Future` é criado, mas nunca recebe `future_set()`.
2. Mesmo corrigindo isso, usar o mesmo thread pool pode causar deadlock.

A correção imediata é:

* criar `FinanceiroArgs`;
* implementar `financeiro_task()`;
* chamar `future_set()`;
* idealmente usar um pool dedicado para o financeiro.`
