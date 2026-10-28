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
ou `halt` feito por um processo gera a interrupção 2 e o SO mata só ele. Um
processo também pode executar `trap` com o número de qualquer quadro, e os
quadros que não são chamadas de sistema precisam lidar com isso. O mais sério
é o quadro 0, que leva ao código de boot: um `trap 0` reiniciava o SO. Agora o
boot só é refeito quando o quadro salvo está todo zerado (o `ip` e o `sp`
antigos em 0), que é como o comando `R` do simulador reinicia a CPU; um
`trap 0` de um processo tem `sp` real e recebe -1, sem reiniciar. Essa
checagem cobre o caso realista, mas não é uma garantia absoluta: um programa
escrito à mão que zere o próprio `sp` e coloque o `trap 0` no endereço 0xFFFE
(onde o `ip` seguinte volta a 0, pelo retorno de 16 bits) ainda cairia no boot.
Os quadros 8, 9 e 10 (console, disco e relógio) não têm defesa parecida: um
`trap` para eles entra no SO e é tratado como se a interrupção tivesse
acontecido. Isso não reinicia nem derruba o sistema, mas conta uma interrupção
a mais, e um `trap 10` ainda avança o relógio do SO: 1000 chamadas `trap 10`
fizeram o tempo total medido saltar para cerca de 6,4 milhões, com a execução
real em 1,48 milhão. Nada disso acontece com os programas entregues, que só
usam `trap 7`; fechar esses casos de vez exigiria o hardware distinguir um
`trap` de uma interrupção, o que o Mancha não oferece, o mesmo limite da
proteção de memória, logo abaixo. O que
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
retorno no `r0` do quadro salvo. Um processo que pede leitura não lê direto
quando já há alguém bloqueado esperando: ele também bloqueia e entra na mesma
disputa, que segue a ordem da tabela de processos, não a ordem em que cada um
bloqueou (como descrito mais abaixo). `SO_ESPERA_PROC` devolve -1 para um pid
que não existe, o que inclui um
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
Uma consequência disso: quando o quantum acaba e o SO reescolhe o mesmo
processo, ele conta uma preempção (mais uma entrada em pronto e em executando),
embora ele não tenha de fato parado de rodar. No circular isso só acontece se
ele estiver sozinho pronto, porque com outros prontos entra o próximo da fila;
na prioridade pode acontecer mesmo com outros prontos, se ele continuar com o
menor valor de prio e for reescolhido.

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
pendente e o tempo pareceria voltar; o SO detecta isso e compensa. Há um limite
que o SO não tem como contornar: se uma única passagem pelo SO demora mais que
um período, o relógio zera duas vezes mas o controlador guarda só uma
interrupção pendente, e um tique se perde, atrasando o tempo medido. Nas cargas
do relatório a passagem mais longa fica logo abaixo de 3000 instruções (a maior
que medi foi de 2982, com prioridade, quantum 1 e período 3000; só o boot, com
cerca de 8500, é maior, mas acontece antes de o relógio começar a contar). A
folga é pequena, e ela cresce com o trabalho feito numa só passagem: quando um
processo morre e vários
estavam esperando por ele, o SO acorda todos de uma vez, e com muitos
esperadores essa passagem pode passar de 3000. Ou seja, o período de 3000 é o
mínimo aceito, mas a medida de tempo só é exata enquanto nenhuma passagem pelo
SO ultrapassar o período; para medições precisas com cargas pesadas, convém usar
um período maior.

As métricas de cada processo ficam guardadas pelo pid, porque precisam
sobreviver à morte dele. Há espaço para 200 pids; depois disso o
`SO_CRIA_PROC` devolve -1. Todos os contadores que crescem com o tempo, tanto
os globais (interrupções de cada tipo e preempções) quanto os por processo
(preempções e número de vezes em cada estado), usam a mesma `struct tempo` de
32 bits, então não estouram numa execução longa; testei um processo preemptado
mais de cem mil vezes e o contador acompanhou. O único contador que continua em
16 bits é o número de amostras usado para a média do tempo de resposta, que só
passaria de 32767 se um mesmo processo desbloqueasse essa quantidade de vezes.
As interrupções são contadas por tipo, e
quando o init morre o tempo do estado atual dos processos ainda vivos é somado
antes da impressão. O tempo em cada estado vai de uma mudança de estado até a
próxima, então inclui o tempo do próprio SO; com isso, para cada processo,
pronto mais bloqueado mais executando dá exatamente o tempo de retorno, o que
dá para conferir em qualquer tabela de métricas do SO. O tempo ocioso segue a
mesma ideia: conta todo o intervalo em que nenhum processo está pronto, desde a
passagem pelo SO em que o último processo bloqueou ou morreu, incluindo o
próprio SO nesse intervalo, então é maior do que só as instruções do laço
ocioso em si.

O custo do SO pesou mais do que eu esperava, porque o mcc não otimiza nada. A
primeira versão gastava umas 1800 instruções por interrupção e, com período de
2000, nenhum processo andava. Passar a cópia de quadro, a comparação de nomes
e as contas de tempo para assembly e evitar percorrer a tabela à toa baixou
para 1100 a 1700 instruções, conforme o escalonador. Mesmo assim, num teste
com período de 1000 o sistema não terminou, com quase todas as instruções no
SO; com 2000 ele termina, mas o relógio já perde tiques, e por isso o
`compilar.sh` só aceita período a partir de 3000.

## Resultados

Usei a mesma carga em todas as execuções. O init cria três processos que só
calculam (`cpu_a`, `cpu_b` e `cpu_c`) e um `interativo`, que lê três linhas e
ecoa cada uma. As linhas chegam pelo console em dois cenários: no rápido, nos
instantes 100 mil, 250 mil e 400 mil, com a CPU ocupada; no lento, em 300 mil,
1,2 milhão e 2,1 milhões, quando os de cálculo já acabaram. Os tempos estão em
instruções, e a porcentagem no SO foi medida pelo executor (inclui umas 105 mil
instruções gastas imprimindo as métricas no final). Tudo dá para refazer com
`bash T2/experimentos.sh`; a simulação é determinística e todos os números,
inclusive a coluna "no SO", saem iguais no Windows e no Linux.

Primeiro variei o escalonador e o quantum, com período de 5000 e entrada
rápida. "Retorno CPU" é a média dos três processos de cálculo.

| escalonador | quantum | tempo total | preempções | no SO | retorno CPU | retorno interativo | resposta interativo |
|---|---|---|---|---|---|---|---|
| simples | - | 682000 | 0 | 38,8% | 405115 | 646812 | - |
| circular | 1 | 873878 | 174 | 50,9% | 810056 | 448507 | 11702 |
| circular | 2 | 769089 | 74 | 45,1% | 721330 | 452087 | 25036 |
| circular | 4 | 721982 | 31 | 42,0% | 672449 | 441757 | 51775 |
| prioridade | 1 | 942247 | 188 | 54,1% | 888619 | 398501 | 1702 |
| prioridade | 2 | 793431 | 76 | 46,5% | 743033 | 403558 | 5103 |
| prioridade | 4 | 738131 | 32 | 43,0% | 684065 | 402182 | 11703 |

O simples termina tudo antes e gasta menos com o SO, porque não troca de
processo à toa, mas o interativo só roda depois que os três de cálculo acabam
e leva 647 mil instruções para terminar, contra uns 450 mil no circular. A
resposta dele aparece como "-" porque, quando ele roda, as linhas já chegaram
e ele nunca bloqueia.

No circular, quantum menor é mais preempção e mais tempo no SO: com quantum 1
quase metade das instruções vai para o SO e o retorno dos processos de cálculo
sobe para 810 mil, mas o interativo responde em 11 mil instruções. Com quantum
4 a resposta vai para 52 mil, porque ao acordar ele fica atrás de três
processos que podem usar até 20 mil instruções cada.

Na prioridade a resposta do interativo foi de 4 a 7 vezes menor que no
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
| circular | 3000 | 959669 | 319 | 54,8% | 903991 | 12000 |
| circular | 5000 | 769089 | 153 | 45,1% | 721330 | 25036 |
| circular | 10000 | 666830 | 66 | 37,9% | 624816 | 56998 |
| prioridade | 3000 | 1025645 | 341 | 57,4% | 962003 | 1000 |
| prioridade | 5000 | 793431 | 158 | 46,5% | 743033 | 5103 |
| prioridade | 10000 | 678339 | 67 | 38,7% | 640085 | 16792 |

Como o quantum é medido em tiques, o período muda ao mesmo tempo o tamanho da
fatia e a quantidade de interrupções. Com 3000 o SO fica com mais da metade do
tempo e tudo demora uns 25% a mais que com 5000. Com 10000 a sobrecarga cai
para pouco mais de um terço, mas a resposta do interativo mais que dobra no
circular e mais que triplica na prioridade, porque a fatia que ele espera
terminar passa a ser de 20 mil instruções.

Por fim, a entrada lenta, com quantum 2 e período 5000.

| escalonador | tempo total | ocioso | preempções | retorno CPU | retorno interativo | resposta interativo |
|---|---|---|---|---|---|---|
| simples | 2136085 | 1452851 | 0 | 404090 | 2100238 | 0 |
| circular | 2141741 | 1365514 | 76 | 692577 | 2102770 | 8901 |
| prioridade | 2142108 | 1340540 | 78 | 719382 | 2102874 | 2234 |

Aqui o sistema fica uns dois terços do tempo ocioso, com o interativo
bloqueado esperando a próxima linha, e o tempo total quase não muda entre os
escalonadores, porque quem manda é a chegada da entrada. O simples fica mais
tempo ocioso justamente por terminar o cálculo antes, e a resposta dele é zero
porque, quando cada linha chega, não sobrou ninguém para disputar a CPU.

Esta é a saída completa da prioridade com quantum 2, período 5000 e entrada
rápida:

```
processos criados:  5
tempo total:        793431
tempo ocioso:       0
preempcoes:         76
interrupcoes:       sistema=111 console=3 disco=0 relogio=158
excecoes:           segmento=0 privilegiada=0 div_zero=0 ilegal=0 pagina=0

pid programa   retorno prmp  npr  pronto  nbl bloqueado  nex executando resposta
  1 init        793431    4    6   30288    1    715730    6      47413        0
  2 cpu_a       757607   24   25  524558    0         0   25     233049        -
  3 cpu_b       724866   22   23  496181    0         0   23     228685        -
  4 cpu_c       746627   23   24  513820    0         0   24     232807        -
  5 interativo  403558    3    7   46073    3    306008    7      51477     5103
```

No circular, com a mesma configuração, o interativo passa 194 mil instruções
pronto esperando na fila; na prioridade, 46 mil, com tempo executando parecido
nos dois. As 111 chamadas de sistema são quase todas escritas, já que a
biblioteca escreve um caractere por chamada.

## Testes

Além das execuções acima, testei com o `executa` os erros das chamadas
(programa ou pid inexistente, matar duas vezes, esperar a si mesmo ou um
processo morto, número de chamada inválido, tabela cheia), processos que fazem
`outb`, `halt` ou dividem por zero, matar um processo bloqueado, três processos
esperando o mesmo, um processo ainda bloqueado quando o init morre, a criação
de mais de 200 processos e o SO parado sem entrada. As 27
combinações de escalonador, quantum (1, 2 e 5) e período (3000, 5000 e 10000)
terminaram com a saída completa e com os tempos de cada processo fechando, e
os extremos aceitos (quantum 30 com período 30000) também. Por fim compilei no
Linux e usei o simulador com interface, digitando as linhas pelo comando `E`.
