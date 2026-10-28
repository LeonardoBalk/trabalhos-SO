# Uma BIOS mínima para o Mancha completo

`bios_mancha` é uma base de firmware para quem for implementar um
sistema operacional sobre o
[simulador do Mancha completo](../simulador_completo/): vetor de
interrupções pronto, handlers padrão de exceção, drivers de console/
disco/relógio, e um mecanismo de chamada de sistema (`trap 7`).

```
seukernel.asm + src/bios.asm --[montador]--> seukernel.mob --[simulador]--> executa
```

## Sumário

1. [Uso](#uso)
2. [O vetor de interrupções](#o-vetor-de-interrupções)
3. [O quadro de 16 palavras](#o-quadro-de-16-palavras)
4. [Interface da BIOS](#interface-da-bios)
5. [pilha_sistema vs. pilha_nucleo](#pilha_sistema-vs-pilha_nucleo)
6. [Chamadas de sistema](#chamadas-de-sistema)
7. [Escalonador (multitarefa preemptiva)](#escalonador-multitarefa-preemptiva)
8. [Escalonador implementado em C](#escalonador-implementado-em-c)
9. [Exemplos](#exemplos)
10. [Validação](#validação)

## Uso

```sh
make                # monta src/bios.asm com cada exemplos/*.asm
```

Para um kernel próprio:

```sh
montador src/bios.asm meukernel.asm -o meukernel.mob
```

O único requisito é que `meukernel.asm` defina um rótulo
`_kernel_inicio`: é para lá que a BIOS salta depois de inicializar,
**com as interrupções externas ainda desligadas** (`SR.D=1`) -- para que
a inicialização do kernel (montar tabela de processos, tabela de
`syscalls`, programar o relógio, etc.) não seja interrompida no meio.
Ligue com a instrução `ei` quando o kernel estiver pronto para receber
interrupções.

## Vetor de interrupções

Os primeiros 128 bytes da memória real (endereço 0) são 16 quadros de 4
palavras (`ip, sp, cs, ds`) cada -- 8 bytes por quadro, um por número de
interrupção (mancha.pdf, secção 7). `ip=0` num quadro significa
"interrupção ignorada" (é assim que memória zerada se comporta por
padrão). `src/bios.asm` já define os 16:

| nº | uso | quem trata |
|---|---|---|
| 0 | início de operação (boot) | `_start` |
| 1 | violação de segmento | pânico (mensagem + para) |
| 2 | instrução privilegiada | pânico |
| 3 | divisão por zero | pânico |
| 4 | instrução ilegal | pânico |
| 5 | ausência de quadro (paginação) | pânico |
| 6 | *livre* | -- |
| 7 | chamada de sistema | despachante de `syscalls` (ver abaixo) |
| 8 | console | driver de console (buffer circular) |
| 9 | disco | (E/S síncrona nesta BIOS -- ver comentário em `bios.asm`) |
| 10 | relógio | conta tiques + chama o escalonador, se instalado |
| 11-15 | *livres* | -- |

Os vetores 6 e 11-15 ficam livres de propósito, para o kernel usar como
quiser (mais chamadas de sistema em vetores diferentes, por exemplo --
embora usar só o 7 com um número de chamada em `r0`, como esta BIOS já
faz, normalmente seja mais prático que ocupar vários vetores).

Cada quadro tem seu próprio `cs`/`ds`: dá para, por exemplo, rodar todo
handler de interrupção em segmentação plana mesmo com processos de
usuário já paginados -- evita o handler ter que lidar com a MMU do outro
processo antes de conseguir tratar a falta dele.

## O quadro de 16 palavras

Toda interrupção/`trap` empilha os 16 registradores do contexto
interrompido, na pilha indicada pelo quadro do vetor (não importa qual
fosse o `sp` de quem foi interrompido -- ver a próxima seção). A ordem,
do endereço **mais baixo** para o mais alto (é a ordem que `rete` espera
para desempilhar, e a mesma usada por `bios_copia_quadro`):

```
sp+0   r0          sp+16  s0 (sr)
sp+2   r1          sp+18  s1
sp+4   r2          sp+20  s2
sp+6   r3          sp+22  s3
sp+8   r4          sp+24  s4 (cs/pt)
sp+10  bp          sp+26  s5 (cl)
sp+12  sp (antigo)  sp+28  s6 (ds)
sp+14  ip (antigo)   sp+30  s7 (dl)
```

Isso é útil, por exemplo, para os handlers de pânico lerem "em que
endereço a exceção aconteceu" (`(sp+14)`) sem precisar de mais nada.

## Interface da BIOS

Convenção de chamada: **diferente** do compilador C do Mancha completo
(que empilha argumentos), as rotinas da BIOS recebem argumentos em `r0`,
`r1`, `r2`, `r3` (nessa ordem) e devolvem o resultado (se houver) em
`r0` -- mais direto para escrever/ler em assembly de mão. `r0`-`r4` são
sempre considerados "sujos" depois de um `call` (nenhum é preservado).

| rotina | argumentos | efeito |
|---|---|---|
| `bios_putc` | r0=caractere | escreve 1 caractere na console |
| `bios_imprime` | r0=endereço de string terminada em 0 | escreve a string |
| `bios_imprime_dec` | r0=valor com sinal | escreve um inteiro decimal |
| `bios_imprime_hex` | r0=valor | escreve 4 dígitos hexadecimais |
| `bios_console_disponivel` | -- | r0 = diferente de 0 se há caractere esperando |
| `bios_console_le` | -- | r0 = próximo caractere (não verifica disponibilidade) |
| `bios_disco_le_setor` | r0=face,r1=trilha,r2=setor,r3=endereço destino | lê 1 setor (512 bytes) do disco 0, bloqueante |
| `bios_disco_escreve_setor` | r0=face,r1=trilha,r2=setor,r3=endereço fonte | escreve 1 setor no disco 0, bloqueante |
| `bios_disco_erro` | -- | r0 = diferente de 0 se a última operação deu erro |
| `bios_copia_quadro` | r0=origem,r1=destino | copia um quadro de 16 palavras (32 bytes) -- ver [Escalonador](#escalonador-multitarefa-preemptiva) |

E três variáveis de dados:

- **`tabela_syscalls`** (16 palavras): endereços dos handlers de
  chamada de sistema, por número (0-15). Uma entrada em 0 (o padrão,
  `_start` já zera tudo) significa "não implementada" -- devolve -1.
- **`retomada_escalonador`** (1 palavra): endereço do escalonador do
  kernel, para ligar multitarefa preemptiva. 0 (o padrão) = sem
  escalonador, o relógio só conta tiques.
- **`contador_tiques`** (1 palavra): incrementado a cada estouro do
  relógio, antes de olhar `retomada_escalonador`.

Só a unidade 0 de disco é endereçável (as portas de face/trilha/setor/
endereço do simulador são fixas na unidade 0, independente do campo de
unidade do byte de operação -- ver
`../simulador_completo/src/dispositivos.c`). E/S de disco aqui é sempre
síncrona (a BIOS espera terminar antes de devolver o controle);
implementar E/S assíncrona de verdade (suspendendo o processo chamador
em vez de girar num laço) é um bom exercício de continuação.

## pilha_sistema vs. pilha_nucleo

**Todo quadro do vetor de interrupções tem um endereço de pilha FIXO.**
A cada interrupção/`trap`, a CPU reescreve `sp` para o valor daquele
campo e empilha o quadro de 16 palavras a partir dali -- não importa
onde `sp` estivesse antes. `src/bios.asm` usa `pilha_sistema` (0xF000)
para *todos* os vetores. Isso significa que `pilha_sistema` só é segura
para o que empilha e desempilha inteiramente **dentro de um único
atendimento de interrupção** -- nada que precise sobreviver a um `rete`
pode morar lá, porque a próxima interrupção (mesmo uma causada pelo
próprio handler, ou uma chamada de sistema feita pelo próprio kernel)
reescreve por cima.

Por isso o kernel roda numa pilha **separada**, `pilha_nucleo` (0xE000):
`_start` troca `sp` para lá antes de fazer qualquer `call` ou chamar
`_kernel_inicio`. Se o seu kernel precisar de mais do que os 4KB entre
`pilha_nucleo` e `pilha_sistema`, ajuste as constantes no topo de
`bios.asm` (só não deixe elas se sobrepor, nem sobrepor a área de
código/dados da BIOS e do kernel).

## Chamadas de sistema

```asm
        ld      r0, NUMERO_DA_CHAMADA
        ld      r1, primeiro_argumento
        ld      r2, segundo_argumento
        trap    7
        ; resultado em r0
```

O kernel instala um handler escrevendo seu endereço em
`tabela_syscalls[numero]`:

```asm
        ld      r0, NUMERO_DA_CHAMADA
        add     r0, r0                  ; índice * 2 (2 bytes por entrada)
        add     r0, tabela_syscalls
        ld      r1, meu_handler
        st      r1, (r0)
```

O handler recebe o número em `r0` (pode ignorar) e os argumentos em
`r1`..`r4`, devolve o resultado em `r0` e termina com `ret` -- **não**
`rete`; quem faz o `rete` de verdade é o despachante da BIOS.

Um detalhe sutil que vale entender (está comentado em `_bios_chamada_sistema`,
em `bios.asm`): `rete` restaura `r0` a partir do **quadro empilhado na
pilha**, não do registrador `r0` ao vivo no momento do `rete`. Então,
para o valor que o handler deixou em `r0` realmente "voltar" para quem
chamou, o despachante da BIOS grava esse valor de volta no lugar certo
da pilha (`st r0, (sp)`) antes do `rete`. Isso já é feito --
só é importante saber *por que* isso é necessário, caso decida escrever
seu próprio mecanismo de retorno de valores em outro contexto (por
exemplo, se seu escalonador precisar "injetar" um valor de retorno
diferente ao acordar um processo bloqueado).

## Escalonador (multitarefa preemptiva)

Ligar multitarefa preemptiva é só:

```asm
        ld      r0, meu_escalonador
        st      r0, (retomada_escalonador)
```

A cada estouro do relógio, se `retomada_escalonador` não for 0, a BIOS
salta (por `jmp`, não `call`) para lá, com `sp` apontando para o quadro
de 16 palavras recém-empilhado do processo interrompido. O escalonador
termina escolhendo um processo (possivelmente outro) e executando
`rete`.

**Por causa do que a seção anterior explica** (todo vetor usa um
endereço de pilha fixo), o quadro do processo interrompido **não fica
guardado "na pilha dele"** -- ele aparece sempre no mesmo lugar
(`pilha_sistema-32`) e seria sobrescrito na próxima interrupção. Um
escalonador de verdade, portanto, não pode só trocar `sp`: ele precisa

1. **copiar** esse quadro para uma área reservada só daquele processo
   (com `bios_copia_quadro`);
2. escolher o próximo processo;
3. **copiar de volta** a área salva do processo escolhido para
   `pilha_sistema-32`;
4. `rete`.

Isso é exatamente o que `exemplos/multitarefa.asm` faz -- leia os
comentários lá, especialmente o de `escalonador:`. A vantagem de tudo
isso (o que faz a troca de contexto sair "de graça" no Mancha, apesar
dessa complicação) é que os 16 registradores -- inclusive `sr` e `sp` --
são sempre salvos/restaurados automaticamente pelo hardware a cada
interrupção; o escalonador só move bytes de um lugar para o outro, não
precisa entender o significado de nenhum deles.

Para "iniciar" o primeiro processo (não há uma interrupção anterior de
onde partir), o kernel monta manualmente um quadro "de mentira" com os
valores iniciais desejados (`ip`=ponto de entrada, `sp`=topo da pilha de
trabalho do processo, `sr`=modo desejado, resto zerado) e faz a mesma
cópia + `rete` que o escalonador faria -- ver `_kernel_inicio` em
`multitarefa.asm`.

**Cuidado com o período do relógio**: `disp_tick` (que conta os tiques)
roda a cada instrução executada de verdade, *inclusive* quando essa
"instrução" foi na verdade o atendimento de uma interrupção -- então o
tempo que o **próprio escalonador** leva para rodar também consome 
tempo do relógio. Se o período configurado for menor que o custo do
escalonador (duas cópias de 16 palavras custam uns 200+ instruções
juntas), o relógio estoura de novo *ainda dentro* do escalonador; como
`D=1` durante todo atendimento de interrupção, essa segunda interrupção
fica pendente e dispara imediatamente assim que `rete` restaura `D=0` --
o processo escolhido nunca chega a executar nem uma instrução antes de
ser trocado de novo, e a "multitarefa" trava sempre no mesmo processo.


Este exemplo roda todo processo em **modo supervisor**, de propósito,
para isolar o conceito de escalonamento do de proteção de memória (que
já está demonstrado em `../simulador_completo/exemplos/protecao.asm` e
`paginacao.asm`). Combinar os dois -- processos de usuário de verdade,
com `S=0` e limites de segmento/paginação configurados por processo -- é
o passo natural seguinte.

## Escalonador implementado em C

`exemplos/multitarefa.asm` (seção anterior) escreve o escalonador todo em
assembly. `exemplos/multitarefa_c/` faz o mesmo exemplo (3 processos,
round-robin, disparado pelo relógio), mas com o **escalonador -- e os
próprios processos -- escritos no subconjunto de C do
[mcc](../../compilador_c/)**. A ideia é mostrar que, uma vez estabelecida
uma pequena "ponte" entre a convenção de chamada da BIOS e a convenção do
mcc, o resto de um kernel de brinquedo pode ser C de verdade -- laços,
ponteiros, structs -- sem precisar reescrever a lógica em assembly.

### Duas convenções de chamada, uma ponte

A BIOS (esta, `bios.asm`) usa uma convenção: argumentos em `r0..r3`,
resultado em `r0`, nada empilhado. O mcc usa outra, documentada em
`../../compilador_c/README.md` ("convenção de chamada"): argumentos
empilhados da direita para a esquerda antes do `call`, resultado em
`r0`, e toda função gerada tem um rótulo `_f_<nome>` com seu próprio
prólogo/epílogo (`push bp` / `ld bp,sp` / ... / `ld sp,bp` / `pop bp` /
`ret`). As duas convenções não são intercambiáveis: chamar uma função
gerada pelo mcc como se fosse uma rotina da BIOS (ou vice-versa) deixa a
pilha desbalanceada.

`exemplos/multitarefa_c/ponte.asm` é a única coisa deste exemplo escrita
à mão em assembly, e ela existe só para:

1. **Traduzir a chamada do escalonador**: `escalonador_trampolim` (é
   isso que fica instalado em `retomada_escalonador`, chamado pela BIOS
   por `jmp`) empilha os 3 argumentos do jeito que o mcc espera e faz um
   `call _f_escalonador` de verdade -- daí em diante, `escalonador()` é
   uma função C comum, compilada pelo mcc a partir de `kernel.c`.
2. **Programar portas de E/S e ligar interrupções** (`outb`, `ei`) --
   coisas que a linguagem C deste compilador não expõe (sem inline
   assembly, sem acesso a porta).
3. **Iniciar o primeiro processo** com um `rete` manual (mesma técnica
   de `_kernel_inicio` em `multitarefa.asm`).

O truque que faz o escalonador em C funcionar sem nenhuma cópia extra:
`escalonador(int *quadro, ...)` recebe o endereço **fixo** onde a CPU
acabou de empilhar o contexto do processo interrompido (o mesmo endereço
de onde `rete` vai ler de volta, ver
[pilha_sistema vs. pilha_nucleo](#pilha_sistema-vs-pilha_nucleo)) como um
ponteiro comum. A função C escreve o resultado **direto nesse endereço**
via `quadro[i] = ...` -- não existe um "valor de retorno" separado para
copiar de volta; quando `escalonador()` retorna, o quadro já está
pronto para o `rete` que vem logo depois, em `ponte.asm`.

### Ponteiro de função

O mcc suporta ponteiro de função (ver `../../compilador_c/README.md`,
"Ponteiros de função") -- a tabela de pontos de entrada dos processos também é
montada em C, direto em `kernel.c`:

```c
void (*pontos_entrada[N_PROCESSOS])(void) = { processoA, processoB, processoC };
```

A montagem do quadro inicial de cada processo (`monta_quadro_inicial`,
usando `pontos_entrada[i]` como `ip`) também é C. O que sobra em
`ponte.asm` é só o que a linguagem realmente não expõe: programar portas
de E/S, ligar interrupções, e o `rete` manual -- referenciando as
variáveis/funções de `kernel.c` só pelos rótulos que o mcc gera
(`_g_processos`, `_g_processo_atual`, `_f_inicializa_processos`,
`_f_escalonador`), sem precisar saber o que tem dentro delas.

Isso mostra bem o que ponteiro de função destrava num kernel escrito
neste subconjunto de C: sem ele, qualquer tabela de despacho (a de
pontos de entrada de processos aqui, mas o mesmo vale para um vetor de
interrupções ou uma tabela de chamadas de sistema escritos em C) tinha
que ser montada em assembly à parte -- com ele, a tabela é só um array
comum, inicializado com os nomes das funções, do jeito mais direto
possível.

### Arquivos

```
exemplos/multitarefa_c/
  kernel.c     escalonador(), os três processos, a tabela de pontos de
                entrada (pontos_entrada[], um array de ponteiros de
                função) e a montagem dos quadros iniciais -- tudo C de
                verdade, compilado pelo mcc
  ponte.asm    a ponte: _kernel_inicio, escalonador_trampolim, e a
                função escreve_caractere (bridge pra outb) -- sem
                nenhum dado estático próprio
  Makefile     mcc kernel.c -o kernel.asm; depois monta
                bios.asm + ponte.asm + kernel.asm juntos
```

## Exemplos

- **`ola_syscall.asm`**: duas chamadas de sistema simples (escrever um
  caractere, somar dois inteiros), sem multitarefa -- para entender o
  mecanismo de despacho isoladamente. Mostra também que um número de
  chamada não implementado devolve -1 em vez de travar o sistema.
- **`multitarefa.asm`**: 3 processos alternando por um escalonador
  round-robin preemptivo disparado pelo relógio, cada um escrevendo sua
  letra através de uma chamada de sistema -- saída
  `ABCABCABCABCABC...` (roda para sempre, de propósito: é uma demo de
  SO, não um programa que "termina").
- **`multitarefa_c/`**: o mesmo exemplo (3 processos, escalonador
  round-robin pelo relógio, saída `ABCABCABC...`), mas com o escalonador
  e os processos escritos em C (compilados pelo mcc) -- ver
  [Escalonador implementado em C](#escalonador-implementado-em-c).

## Validação

`bios.asm` sozinho monta sem erros (só falta `_kernel_inicio`, como
esperado, quando montado sem nenhum kernel). 

- `ola_syscall.mob`: console = `"oi!\n42\n-1\n"` (a syscall de escrita
  chamada três vezes monta "oi!", a de soma devolve 40+2=42, e o número
  9 -- não implementado -- devolve -1), pára após 367 instruções. ✓
- `multitarefa.mob`: console = `"ABCABCABCABC..."` intercalado de forma
  perfeita, estável por 2 milhões de instruções sem travar; confirmado
  também interativamente no simulador real (comandos `D0`/`C`), mostrando
  `RODANDO` e a mesma saída após 916 mil instruções. ✓ (nunca "pára" --
  é uma demo de SO de propósito, não um programa que termina)
- Handler de pânico (`div r0,r1` com `r1=0`, num kernel de teste avulso):
  imprime `"EXCECAO: divisao por zero em ip=<endereço>"` e pára de
  verdade (`halt`). ✓
- `multitarefa_c.mob`: console = `"ABCABCABCABC..."` intercalado de
  forma perfeita, estável por 2 milhões de instruções sem travar;
  confirmado também interativamente no simulador real, mostrando
  `RODANDO` e a mesma saída após 240 mil instruções. ✓ (revalidado depois
  da tabela de processos passar a ser montada em C com
  `pontos_entrada[]`, um array de ponteiros de função -- mesmo resultado,
  ver [Ponteiro de função](#ponteiro-de-função))

## Organização do código

```
src/
  bios.asm          vetor de interrupções, handlers de exceção,
                      drivers de console/disco/relógio, despachante de
                      chamadas de sistema, rotinas de impressão
exemplos/
  ola_syscall.asm    chamadas de sistema isoladas, sem multitarefa
  multitarefa.asm     3 processos + escalonador round-robin preemptivo,
                        tudo em assembly
  multitarefa_c/       mesmo exemplo, escalonador e processos em C
    kernel.c            escalonador() e os processos (compilado pelo mcc)
    ponte.asm             a ponte entre a convenção da BIOS e a do mcc
    Makefile
```
