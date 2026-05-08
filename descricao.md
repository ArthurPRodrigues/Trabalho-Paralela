Este trabalho explora o uso de programação paralela e padrões de projeto para programação multithread. O objetivo é explorar conceitos de paralelismo, a adoção de abstrações e a praticidade oferecida por padrões de projeto no desenvolvimento de aplicações concorrentes.  

Você deverá implementar um protótipo para simular fluxos de trabalhos relacionados à venda de itens (ex. vendas de computadores). Os atores envolvidos são: 
● clientes: geram novos pedidos de compra concorrentemente. Cada pedido deve ser postado em uma fila de pedidos; 
● A partir da fila de pedidos, há um fluxo que deve ser executado pelo sistema de vendas: 
○ Cada pedido precisa ter uma validação do pedido (ex. se usuário está 
cadastrado corretamente e se o pedido reflete a um item válido); 
○ Além disso, cada pedido precisa ser validado pela operadora financeira (para efetuar a cobrança no cartão de crédito); 
○ Cada pedido com cadastro e operação financeira validados deve passar para a logística, que será responsável por realizar a entrega e, posteriormente, atualizar o status do pedido, encerrando a venda. 
○ Cada etapa de validação e logística pode representar uma taxa de falha configurável, indicando que um cadastro estava incorreto, que a operação financeira não foi autorizada ou que houve avaria na entrega do pedido.  Observação: os dados e validações são apenas simulações, não necessitando seguir regras específicas para validação de cadastro (CPF, etc.) ou de operação financeira (ex. cartão inválido). 
Você deve projetar quais elementos de processamento, estruturas e padrões de projeto serão utilizados. Entretanto, você deve utilizar pelo menos 3 padrões de projeto para programação paralela. 

Requisitos e avaliação 
Os requisitos específicos são: 
● Implementar o protótipo utilizando programação paralela e explorando o uso de múltiplos núcleos de processamento em uma arquitetura multiprocessada; 
● Deve ser entregue: o código com a descrição (ex. arquivo readme) e detalhes para compilação, implantação e execução da aplicação. LINGUAGEM C. apresentar explicações complementares sobre estruturas de paralelismo utilizadas, que possam diferir de estruturas comuns em Threads POSIX; 
● Relatório com (i) a explicação sobre a arquitetura de software utilizada e da escolha e adoção dos padrões escolhidos; (ii) ilustração de casos de uso utilizados para teste da solução, com explicação sobre as saídas de execução; 