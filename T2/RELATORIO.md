# Relatório do T2

## O SO

Escrevi o SO quase todo em C, compilado pelo mcc. O assembly ficou só no que o
C do mcc não alcança: `so/ponte.asm` mexe no vetor de interrupções, acessa as
portas, troca de pilha e dá o `rete`, e `so/tempo.asm` tem as contas de tempo,
que em C ficavam lentas demais.

Toda interrupção entra pelo mesmo lugar. No `_kernel_inicio` a ponte troca, no
vetor, os tratadores das exceções (1 a 5), da chamada de sistema (7), do
console (8), do disco (9) e do relógio (10) por tratadores meus. Cada um só põe
o número da interrupção em `r0` e pula para uma entrada comum, que muda para a
`pilha_nucleo` e chama `so_trata_interrupcao`. Ali o SO salva o quadro do
processo interrompido na tabela, trata o que causou a interrupção, resolve as
pendências de E/S, escalona e copia o quadro do escolhido de volta para
`pilha_sistema-32`, de onde a ponte faz o `rete`. Se ninguém está pronto, roda
um laço vazio em modo supervisor com as interrupções ligadas até a próxima
interrupção.

Os processos rodam com `sr` zerado, sem modo supervisor e sem E/S; um `outb`
ou `halt` feito por um processo gera a interrupção 2 e o SO mata só ele. O que
não existe é proteção de memória: os limites de segmento ficam zerados, então
um processo pode escrever em qualquer endereço, inclusive no vetor de
interrupções, e com isso desviar uma interrupção para o próprio código e
rodá-lo em modo supervisor. Com segmentação isso não tem como ser fechado aqui,
porque o vetor fica fixo no endereço 0, os limites de segmento só cortam
endereços acima de um valor e os programas são montados em endereços absolutos
junto com o SO. A saída seria paginação, com as páginas do núcleo só para
leitura na tabela dos processos. O enunciado pede a proteção de modo e de E/S
(S=0 e I=0), que está feita, e deixei a de memória fora deste trabalho.

Não usei a `tabela_syscalls` da BIOS. O despachante dela grava o `r0` no
quadro depois que o tratador retorna, e na parte II uma chamada pode bloquear o
processo e trocar o quadro pelo de outro, que teria o `r0` estragado por essa
gravação. Além disso, o tratador de console da BIOS não avisa o SO quando chega
um caractere, e as exceções dela param a máquina inteira. Da BIOS continuei
usando o vetor, o `_start`, as pilhas e a `bios_copia_quadro`, e mantive a
convenção do enunciado: `trap 7`, número da chamada em `r0` e argumento em
`r1`.

Os programas são montados junto com o SO e o `SO_CRIA_PROC` recebe o nome de
um deles. Cada processo tem uma pilha de 512 bytes com o endereço de uma rotina
que chama `SO_MATA_PROC(0)` no topo, para o caso de a função principal
retornar. A biblioteca C (`putchar`, `puts`, `print_int`) foi adaptada para
usar as chamadas de sistema em vez das portas.

## Partes I e II

A `struct processo` guarda pid, estado, motivo do bloqueio, o pid esperado, a
prioridade e o quadro de 16 palavras, e a tabela tem 8 entradas. `atual`
aponta a entrada em execução e fica em -1 quando não tem ninguém. O pid vem de
um contador que só cresce, então uma entrada liberada é reaproveitada com outro
pid. O escalonador da parte I é o do enunciado: o atual continua se ainda
estiver pronto, senão entra o primeiro pronto da tabela.

`SO_LE` e `SO_ESCREVE` fazem a E/S na hora se o console está pronto e não há
ninguém bloqueado esperando por ele; se não, o processo fica bloqueado com o
motivo anotado, e a cada interrupção o SO confere o estado do console, completa
as operações pendentes, desbloqueia quem foi atendido e coloca o valor de
retorno no `r0` do quadro salvo. A condição de ninguém estar esperando evita
que um processo que chegou depois passe na frente de quem já estava bloqueado.
`SO_ESPERA_PROC` devolve -1 para um pid que não existe, o que inclui um
processo que já morreu, e para o próprio processo; nos outros casos bloqueia,
e quando um processo morre o SO acorda todos que esperavam por ele.

No simulador a saída do console está sempre pronta, então a escrita nunca
bloqueia na prática; o caminho de bloqueio na escrita existe no código para o
caso de o console não estar pronto. As mensagens do próprio SO, como a tabela
de métricas, também esperam o console ficar pronto antes de escrever.

Como o console é um só, quando vários processos leem ao mesmo tempo cada
caractere vai para o primeiro bloqueado na ordem da tabela. Rodei com três
leitores e nenhum caractere se perdeu, mas as letras de uma linha se espalharam
entre eles.

## Parte III

No circular, quem é criado, desbloqueia ou perde o quantum vai para o fim da
fila, e o escalonador pega o primeiro. `quantum_restante` recebe o quantum
quando um processo é escolhido e diminui a cada tique do relógio. O enunciado
manda reiniciar o quantum só quando entra um processo diferente do
interrompido; eu reinicio a cada escolha, porque seguindo à risca um processo
sozinho na fila ficaria com o quantum zerado e seria interrompido a cada tique.

O mcc não tem ponto flutuante, então a prioridade é guardada vezes 1000. Um
processo novo começa com 500 e, ao perder o processador, recebe
`prio = (prio + t_exec * 1000 / QUANTUM) / 2`, com `t_exec` igual ao quantum
menos o que sobrou. O escalonador escolhe o menor valor entre os prontos e, no
empate, segue a ordem da fila. Como o `int` tem 16 bits, a conta `t_exec *
1000` só cabe com quantum até 32, e o `compilar.sh` aceita quantum de 1 a 30.

O mcc também só tem `int` de 16 bits, e os tempos passam de centenas de
milhares de instruções. Criei uma `struct tempo` com dois inteiros, valendo
`alto * 10000 + baixo`, e fiz em assembly a soma, a subtração e a divisão de
32 bits usada na média do tempo de resposta. O instante
atual vem do relógio: o SO soma o período a cada interrupção do relógio e
acrescenta o contador. O período fica limitado a 30000 para o contador caber
no `int`. Se o contador zera enquanto o SO está rodando, a interrupção fica
pendente e o tempo pareceria voltar; o SO detecta isso e compensa. Se uma única
passagem pelo SO demorasse mais que um período, um tique se perderia, porque o
controlador guarda só uma interrupção pendente. A passagem mais longa hoje é a
morte de um processo, com umas 2700 instruções, então isso não acontece com
período de 3000 ou mais.

As métricas de cada processo ficam guardadas pelo pid, porque precisam
sobreviver à morte dele. Há espaço para 200 pids; depois disso o
`SO_CRIA_PROC` devolve -1. As interrupções são contadas por tipo, e quando o
init morre o tempo do estado atual dos processos ainda vivos é somado antes da
impressão. O tempo em cada estado vai de uma mudança de estado até a próxima,
então inclui o tempo do próprio SO; com isso, para cada processo, pronto mais
bloqueado mais executando dá exatamente o tempo de retorno, o que dá para
conferir em qualquer tabela de métricas do SO.

O custo do SO pesou mais do que eu esperava, porque o mcc não otimiza nada. A
primeira versão gastava umas 1800 instruções por interrupção e, com período de
2000, nenhum processo andava. Passar a cópia de quadro, a comparação de nomes
e as contas de tempo para assembly e evitar percorrer a tabela à toa baixou
para 1100 a 1700 instruções, conforme o escalonador. Mesmo assim, num teste
com período de 1000 o sistema não terminou, com quase todas as instruções no
SO, e por isso o `compilar.sh` só aceita período a partir de 2000.

## Resultados

Usei a mesma carga em todas as execuções. O init cria três processos que só
calculam (`cpu_a`, `cpu_b` e `cpu_c`) e um `interativo`, que lê três linhas e
ecoa cada uma. As linhas chegam pelo console em dois cenários: no rápido, nos
instantes 100 mil, 250 mil e 400 mil, com a CPU ocupada; no lento, em 300 mil,
1,2 milhão e 2,1 milhões, quando os de cálculo já acabaram. Os tempos estão em
instruções, e a porcentagem no SO foi medida pelo executor (inclui umas 80 mil
instruções gastas imprimindo as métricas no final). Tudo dá para refazer com
`bash T2/experimentos.sh`; a simulação é determinística e o Linux deu os mesmos
números que o Windows.

Primeiro variei o escalonador e o quantum, com período de 5000 e entrada
rápida. "Retorno CPU" é a média dos três processos de cálculo.

| escalonador | quantum | tempo total | preempções | no SO | retorno CPU | retorno interativo | resposta interativo |
|---|---|---|---|---|---|---|---|
| simples | - | 677162 | 0 | 38,3% | 403227 | 642881 | - |
| circular | 1 | 852174 | 170 | 49,7% | 790445 | 447789 | 11409 |
| circular | 2 | 756458 | 73 | 44,1% | 713983 | 452379 | 24743 |
| circular | 4 | 716758 | 32 | 41,4% | 675707 | 456211 | 56411 |
| prioridade | 1 | 918194 | 183 | 52,9% | 865760 | 398174 | 1491 |
| prioridade | 2 | 783226 | 75 | 45,8% | 736978 | 403706 | 4743 |
| prioridade | 4 | 731034 | 32 | 42,4% | 679763 | 402390 | 11437 |

O simples termina tudo antes e gasta menos com o SO, porque não troca de
processo à toa, mas o interativo só roda depois que os três de cálculo acabam
e leva 643 mil instruções para terminar, contra uns 450 mil no circular. A
resposta dele aparece como "-" porque, quando ele roda, as linhas já chegaram
e ele nunca bloqueia.

No circular, quantum menor é mais preempção e mais tempo no SO: com quantum 1
quase metade das instruções vai para o SO e o retorno dos processos de cálculo
sobe para 790 mil, mas o interativo responde em 11 mil instruções. Com quantum
4 a resposta vai para 56 mil, porque ao acordar ele fica atrás de três
processos que podem usar até 20 mil instruções cada.

Na prioridade a resposta do interativo foi de 5 a 8 vezes menor que no
circular. Quando ele desbloqueia, o SO não interrompe quem está rodando, mas
no fim dessa fatia ele passa na frente dos outros, porque quase nunca gasta o
quantum inteiro e o valor de `prio` dele fica baixo. No circular ele vai para o
fim da fila e espera a fatia de todo mundo. Por isso, na prioridade, a resposta
cresce com o quantum só na medida da fatia de um processo, e no circular cresce
com a de três. Na saída também se vê a diferença: no circular o eco sai picado
no meio das letras dos outros, na prioridade a linha sai inteira. O tempo total
fica um pouco maior, porque escolher pela prioridade custa percorrer a fila.

Depois variei o período do relógio, com quantum 2 e entrada rápida.

| escalonador | período | tempo total | int. relógio | no SO | retorno CPU | resposta interativo |
|---|---|---|---|---|---|---|
| circular | 3000 | 937189 | 312 | 53,8% | 875160 | 16697 |
| circular | 5000 | 756458 | 151 | 44,1% | 713983 | 24743 |
| circular | 10000 | 661914 | 66 | 37,3% | 616290 | 56409 |
| prioridade | 3000 | 996799 | 332 | 56,2% | 944429 | 3217 |
| prioridade | 5000 | 783226 | 156 | 45,8% | 736978 | 4743 |
| prioridade | 10000 | 673100 | 67 | 38,2% | 637051 | 16613 |

Como o quantum é medido em tiques, o período muda ao mesmo tempo o tamanho da
fatia e a quantidade de interrupções. Com 3000 o SO fica com mais da metade do
tempo e tudo demora uns 25% a mais que com 5000. Com 10000 a sobrecarga cai
para pouco mais de um terço, mas a resposta do interativo mais que dobra no
circular e mais que triplica na prioridade, porque a fatia que ele espera
terminar passa a ser de 20 mil instruções.

Por fim, a entrada lenta, com quantum 2 e período 5000.

| escalonador | tempo total | ocioso | preempções | retorno CPU | retorno interativo | resposta interativo |
|---|---|---|---|---|---|---|
| simples | 2134975 | 1456603 | 0 | 401991 | 2100695 | 0 |
| circular | 2139466 | 1375847 | 74 | 682051 | 2103096 | 8803 |
| prioridade | 2139756 | 1353085 | 76 | 704209 | 2103191 | 2137 |

Aqui o sistema fica uns dois terços do tempo ocioso, com o interativo
bloqueado esperando a próxima linha, e o tempo total quase não muda entre os
escalonadores, porque quem manda é a chegada da entrada. O simples fica mais
tempo ocioso justamente por terminar o cálculo antes, e a resposta dele é zero
porque, quando cada linha chega, não sobrou ninguém para disputar a CPU.

Esta é a saída completa da prioridade com quantum 2, período 5000 e entrada
rápida:

```
processos criados:  5
tempo total:        783226
tempo ocioso:       0
preempcoes:         75
interrupcoes:       sistema=111 console=3 disco=0 relogio=156
excecoes:           segmento=0 privilegiada=0 div_zero=0 ilegal=0 pagina=0

pid programa   retorno prmp  npr  pronto  nbl bloqueado  nex executando resposta
  1 init        783226    4    7   30777    2    704639    7      47810        0
  2 cpu_a       742026   23   24  514047    0         0   24     227979        -
  3 cpu_b       722679   22   23  496244    0         0   23     226435        -
  4 cpu_c       746230   23   24  515525    0         0   24     230705        -
  5 interativo  403706    3    7   45448    3    307961    7      50297     4743
```

No circular, com a mesma configuração, o interativo passa 194 mil instruções
pronto esperando na fila; na prioridade, 45 mil, com tempo executando parecido
nos dois. As 111 chamadas de sistema são quase todas escritas, já que a
biblioteca escreve um caractere por chamada.

## Testes

Além das execuções acima, testei com o `executa` os erros das chamadas
(programa ou pid inexistente, matar duas vezes, esperar a si mesmo ou um
processo morto, número de chamada inválido, tabela cheia), processos que fazem
`outb`, `halt` ou dividem por zero, matar um processo bloqueado, três processos
esperando o mesmo, um processo ainda bloqueado quando o init morre, a criação
de mais de 200 processos e o SO parado sem entrada. As 27
combinações de escalonador, quantum (1, 2 e 5) e período (2000, 5000 e 10000)
terminaram com a saída completa e com os tempos de cada processo fechando, e
os extremos aceitos (quantum 30 com período 30000) também. Por fim compilei no
Linux e usei o simulador com interface, digitando as linhas pelo comando `E`.
