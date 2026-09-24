# Processador Mancha

**Autor original:** Marcelo Pasin — Universidade Federal de Santa Maria (26 de agosto de 2004)
**Documento de referência preparado a partir do texto original**, com correções de digitação e pequenos ajustes de redação.

---

## 1. Introdução

Compreender o funcionamento dos computadores modernos, cada dia mais complexos, é uma tarefa difícil para um estudante principiante em informática. Em especial, um estudante iniciante desconhece a maioria dos conceitos de arquitetura e organização de computadores. O processador Mancha foi concebido com o objetivo de facilitar o ensino de arquitetura de computadores. Ele é um processador bastante simples e didático que possui a maioria das características de um processador moderno. Para ele foram desenvolvidas diversas ferramentas, como um simulador, um montador e um ligador (havendo também um compilador C em desenvolvimento), de modo a permitir a construção e a execução de programas. Isso permite desenvolver trabalhos práticos bastante realistas para disciplinas como arquitetura e organização de computadores e sistemas operacionais.

Este texto tem o objetivo de descrever o processador Mancha e as ferramentas desenvolvidas para ele.

### Modelo Completo e Modelo Mínimo

O Mancha é um processador hipotético de 16 bits de propósito geral, concebido inicialmente como ferramenta didática da disciplina de Programação de Sistemas do Curso de Informática (hoje Ciência da Computação) da Universidade Federal de Santa Maria. Ele possui dois modelos, um completo e outro reduzido (mínimo). O modelo completo implementa a maioria das características de um processador moderno, mantendo níveis baixos de complexidade, de forma a permitir sua inteira compreensão por um aluno principiante no assunto.

O modelo reduzido, chamado **Mancha Mínimo**, implementa um subconjunto das instruções disponíveis no modelo completo. Geralmente este modelo é usado em disciplinas de organização de computadores, onde em aulas práticas se faz a implementação de um pequeno processador usando blocos funcionais digitais. O processador completo executa todas as instruções do modelo reduzido, permitindo assim utilizar-se o mesmo conjunto de ferramentas em mais de uma disciplina.

---

## 2. Modos de Operação

Como todo processador moderno, o Mancha possui dois modos de operação: **modo usuário** e **modo supervisor**.

- **Modo usuário:** o processador não pode executar todas as instruções de seu conjunto, nem pode acessar toda a memória disponível. Este modo foi criado para permitir a execução segura de instruções de programas escritos por usuários do computador.
- **Modo supervisor:** o processador pode executar qualquer instrução definida em seu conjunto. Serve para executar o programa do sistema operacional.

A distinção entre usuário e supervisor serve para proteger o sistema do mau uso por programas de usuário. Se um destes programas estiver errado e modificar indevidamente conteúdos de memória ou manipular dispositivos de entrada e saída, ele pode vir a prejudicar os outros programas em execução. Assim, em um computador moderno, diversos programas podem estar em execução, cada um deles operando sem interferir nos outros.

O único programa que pode interferir no funcionamento dos outros é o sistema operacional, já que essa é a sua função. Todas as operações que podem por em risco o funcionamento do computador são feitas pelo sistema operacional (ainda que por pedido dos programas de usuário). Considera-se que o sistema operacional não vai operar indevidamente e, por isso, não vai prejudicar a operação dos outros programas — claro que isso não evita que sistemas operacionais continuem sendo lançados com erros.

---

## 3. Conjunto de Registradores

A vasta maioria dos processadores existentes possui em seu interior um conjunto de registradores. Este conjunto serve como depósito para dados dentro do processador enquanto estes dados estão sendo operados. Os processadores têm instruções para carregar (*load*) dados da memória e guardá-los em registradores internos, para armazenar (*store*) dados de registradores na memória, bem como instruções para operar sobre dados contidos em registradores.

O Mancha possui **16 registradores**, oito de usuário e oito de supervisor, todos com 2 bytes (16 bits). Os registradores de supervisor somente podem ser acessados em modo supervisor.

### Registradores de usuário

| Código | Nome | Descrição |
|:---:|:---:|---|
| `000` | `r0` | uso geral |
| `001` | `r1` | uso geral |
| `010` | `r2` | uso geral |
| `011` | `r3` | uso geral |
| `100` | `r4` | uso geral |
| `101` | `bp` | uso geral, ponteiro de base |
| `110` | `sp` | ponteiro da pilha |
| `111` | `ip` | ponteiro de instruções |

Os registradores `r0` a `r5` podem ser usados em qualquer operação de acesso a memória, aritmética ou de lógica — são os registradores de usuário de **propósito geral**. O registrador `bp` (*base pointer*, ponteiro de base) também é de propósito geral, mas é tradicionalmente usado em programas compilados que implementem chamadas de procedimento, como **ponteiro do quadro** de ativação, contendo as variáveis locais do procedimento em execução.

Os registradores `sp` e `ip` são registradores especiais. Existem instruções que fazem uso específico deles sem sequer mencioná-los explicitamente, embora também possam ser usados em quaisquer operações comuns de memória, aritmética ou lógica.

- `ip` é o **ponteiro de instruções** (*instruction pointer*), também chamado de contador de programa. O número nele contido é usado como endereço de busca para instruções na memória. A cada busca ele é incrementado, permitindo que o processador execute programas compostos por instruções sequencialmente dispostas na memória.
- `sp` é o **ponteiro da pilha** (*stack pointer*) do processador. Quando um procedimento é chamado, o valor do contador de programa é guardado na memória, na posição dada por `sp`. O `ip` recebe então um novo valor, correspondente à primeira instrução do procedimento chamado. Para retornar à posição original, o processador carrega `ip` a partir da posição de memória indicada por `sp`. A cada chamada, `sp` é decrementado, sendo novamente incrementado no retorno — funcionando como ponteiro de uma pilha de valores de `ip`.

### Registrador de estado (`sr`) e registradores de supervisor

| Código | Nome | Descrição |
|:---:|:---:|---|
| `000` | `sr` | registrador de estado |
| `001` | `s1` | uso geral |
| `010` | `s2` | uso geral |
| `011` | `s3` | uso geral |
| `100` | `cs` | início do segmento de código |
| `101` | `cl` | tamanho do segmento de código |
| `110` | `ds` | início do segmento de dados |
| `111` | `dl` | tamanho do segmento de dados |

O Mancha possui um **registrador de estado**, `sr`, que contém bits sinalizando várias condições. Se alguma condição for verdadeira, o bit correspondente estará ligado:

| Bit | Significado |
|---|---|
| `S` | modo supervisor ativado |
| `P` | paginação ativada |
| `I` | processador pode usar instruções `in` e `out` |
| `D` | interrupções desativadas |
| `N` | o resultado da última operação é negativo |
| `O` | o resultado da última operação não cabe em 16 bits (*overflow*/*underflow*) |
| `Z` | o resultado da última operação é zero |
| `C` | bit de transporte, "vai um" (*carry*) |

---

## 4. Modos de Endereçamento

Para especificar qual dado será usado em uma operação aritmética ou lógica, o Mancha possui **sete modos de endereçamento**, designados por um código de 3 bits e geralmente associados a um registrador de usuário.

| Nome | Código | Exemplo | Descrição |
|---|:---:|---|---|
| Direto | `000` | `ld r0,r2` | o dado é o conteúdo de um registrador |
| Indireto | `001` | `ld r0,(r2)` | o dado é uma palavra de memória apontada por um registrador de índice |
| Pós-incremento | `010` | `ld r0,(r2+)` | o dado é apontado por um registrador de índice, que é incrementado após o acesso (desempilhar / *pop*) |
| Pré-decremento | `011` | `ld r0,(-r2)` | o registrador de índice é decrementado antes do acesso (empilhar / *push*) |
| Imediato | `100` | `ld r0,0002` | o dado é uma palavra armazenada imediatamente após a instrução |
| Absoluto | `101` | `ld r0,(0002)` | o dado é a palavra apontada pela palavra armazenada logo após a instrução |
| Deslocamento | `110` | `ld r0,(r1+0002)` | o endereço é a soma da palavra imediata após a instrução com o conteúdo de um registrador de índice |

Nos modos de incremento/decremento, se a operação for de palavra o registrador de índice é ajustado de dois em dois; se for de byte, de um em um. Os modos imediato, absoluto e deslocamento fazem com que a instrução ocupe uma palavra adicional de 16 bits (o valor imediato).

---

## 5. Conjunto de Instruções

As instruções do Mancha cobrem movimentação de dados, controle de execução, operações aritméticas e operações lógicas. Os argumentos das instruções usam a seguinte notação:

- *REG*: um registrador de usuário (`r0` a `r7`, `bp`, `sp`, `ip`);
- *SUP*: um registrador de supervisor (`s0` a `s7`, `sr`, `cs`, `cl`, `ds`, `dl`);
- *IMED*: um valor imediato de 16 bits;
- *DATA*: um dado em um dos modos de endereçamento (*REG*, `(REG)`, `(REG+)`, `(-REG)`, *IMED*, `(IMED)` ou `(REG+IMED)`);
- *IM₁₀*: valor de 10 bits com sinal (-512 a 511);
- *IM₆*: valor de 6 bits com sinal (-32 a 31);
- *COND*: uma condição de desvio (`Z`, `EQ`, `C`, `LO`, `O`, `N`, `NP`, `NZ`, `NE`, `NC`, `HS`, `NO`, `NN`, `P`, `GE`, `LT`, `GT`, `LE`, `HI`, `LS`).

### 5.1 Movimentação de dados

| Instrução | Operandos | Descrição |
|---|---|---|
| `ld` / `ldb` | `REG, DATA` | carrega palavra / byte em um registrador |
| `lds` | `SUP, DATA` | carrega registrador de supervisor (privilegiada) |
| `ldq` | `REG, IM₁₀` | carga rápida de valor imediato de 10 bits com sinal |
| `st` / `stb` | `REG, DATA` | armazena palavra / byte de um registrador em memória |
| `sts` | `SUP, DATA` | armazena registrador de supervisor (não privilegiada) |
| `push` | `REG` | empilha registrador (`sp` decrementado antes) |
| `pushs` | `SUP` | empilha registrador de supervisor (não privilegiada) |
| `pop` | `REG` | desempilha registrador (`sp` incrementado depois) |
| `pops` | `SUP` | desempilha registrador de supervisor (privilegiada) |
| `in` / `inb` | `REG, DATA` | lê dado de porta de entrada (privilegiada) |
| `out` / `outb` | `REG, DATA` | escreve dado em porta de saída (privilegiada) |
| `swap` / `swapb` | `REG, DATA` | troca valores entre *REG* e *DATA* de forma atômica (ininterrupta) — usado para exclusão mútua |

### 5.2 Controle de execução

| Instrução | Operandos | Descrição |
|---|---|---|
| `jmp` | `DATA` | desvio absoluto |
| `jmpc` | `COND, DATA` | desvio absoluto condicional |
| `bra` | `DATA` | desvio relativo ao `ip` |
| `brac` | `COND, IM₆` | desvio relativo condicional |
| `braq` | `IM₁₀` | desvio relativo rápido (incondicional) |
| `skip` | `COND` | salta os próximos 4 bytes se a condição for satisfeita (equivale a `brac COND,4`) |
| `call` | `DATA` | chamada de procedimento (empilha endereço de retorno) |
| `ret` | — | retorno de procedimento |
| `trap` | `IM₆` | causa uma exceção/interrupção de software (chamada de sistema) |
| `rete` | — | retorno de exceção |
| `di` / `ei` | — | desabilita / habilita interrupções (privilegiadas) |
| `halt` | — | para a execução do processador (privilegiada) |

### 5.3 Operações aritméticas

| Instrução | Descrição |
|---|---|
| `add` / `addb` | soma *DATA* a *REG* |
| `addc` | soma com transporte (*carry*), para aritmética de mais de 16 bits |
| `addq` | soma rápida de valor imediato de 10 bits |
| `sub` / `subb` | subtrai *DATA* de *REG* |
| `cmp` / `cmpb` | comparação aritmética entre *REG* e *DATA* (não altera *REG*) |
| `mul` | multiplica *REG* por *DATA* |
| `div` | divide *REG* por *DATA* |

### 5.4 Operações lógicas

| Instrução | Descrição |
|---|---|
| `and` / `andb` | E lógico |
| `or` / `orb` | OU lógico |
| `xor` / `xorb` | OU-exclusivo lógico |
| `shl` / `shlb` | deslocamento à esquerda |
| `shr` / `shrb` | deslocamento à direita |

Instruções terminadas em `b` operam apenas sobre os 8 bits menos significativos de *DATA* e *REG*. Nas instruções aritméticas e lógicas gerais os bits `N`, `O`, `Z` e `C` de `sr` são atualizados conforme o resultado.

### 5.5 Condições para desvio

| Nome | Código | Descrição |
|:---:|:---:|---|
| `Z` / `EQ` | `0000` | resultado zero / `REG = DATA` |
| `C` / `LO` | `0001` | houve transporte (*carry*) / `REG < DATA` (sem sinal) |
| `O` | `0010` | houve perda de bits (*overflow*) |
| `N` / `NP` | `0011` | resultado negativo / resultado não positivo |
| `NZ` / `NE` | `0100` | resultado não é zero / `REG ≠ DATA` |
| `NC` / `HS` | `0101` | não houve transporte / `REG ≥ DATA` (sem sinal) |
| `NO` | `0110` | não houve perda de bits |
| `P` | `0111` | resultado positivo |
| `GE` | `1000` | `REG ≥ DATA` (com sinal) |
| `LT` | `1001` | `REG < DATA` (com sinal) |
| `GT` | `1010` | `REG > DATA` (com sinal) |
| `LE` | `1011` | `REG ≤ DATA` (com sinal) |
| `HI` | `1100` | `REG > DATA` (sem sinal) |
| `LS` | `1101` | `REG ≤ DATA` (sem sinal) |

---

## 6. Formato das Instruções

O Mancha é um processador **big-endian**: ao buscar palavras de instrução na memória, a parte mais significativa (bits 15 a 8) vem do endereço da instrução e a parte menos significativa (bits 7 a 0) vem do endereço seguinte. Cada instrução ocupa 16 bits (uma palavra), podendo ganhar uma palavra adicional quando usar o modo de endereçamento imediato ou absoluto.

Os dois bits mais significativos da instrução definem seu **formato**, dos quais existem quatro:

1. **Formato registrador (`00`)** — instruções de movimentação de dados e operações aritméticas/lógicas de usuário. Campos: bits 15-14 = `00`; bits 13-10 = *codop*; bit 9 = tamanho (0 = palavra, 1 = byte); bits 8-6 = *REG*; bits 5-3 = modo de endereçamento (*MOD*); bits 2-0 = registrador associado ao modo (*REG* do *DATA*).
2. **Formato especial (`01`)** — movimentação de registradores de supervisor e algumas instruções de controle (`call`, `mul`, `div`, `addc`, `lds`, `sts`). Sempre opera com palavras (16 bits).
3. **Formato condicional (`10`)** — desvios condicionais e instruções de controle do processador (`trap`, `brac`, `rete`, `halt`, `di`, `ei`, `jmpc`), sem uso de modos de endereçamento. Traz um campo *COND* de 4 bits e um valor imediato *IM₆*.
4. **Formato rápido (`11`)** — apenas `ldq` e `addq`, que usam um valor imediato de 10 bits (*IM₁₀*) sem modos de endereçamento.

Algumas instruções (como `jmp`, `bra`, `ret`, `push` e `pop`) são codificadas no formato registrador, mas operam sobre registradores e modos de endereçamento pré-determinados — por exemplo, `jmp DATA` é equivalente a `ld ip, DATA`.

---

## 7. Interrupções

Todo processador moderno possui algum sistema de tratamento de interrupções, que permite interromper a execução de um programa para atender uma sequência urgente de instruções, retornando à execução normal em seguida. Interrupções costumam estar relacionadas a operações de entrada e saída ou a condições internas críticas do processador.

### Tipos de interrupção

Existem 16 números de interrupção possíveis (0 a 15):

- **Interrupções internas (0 a 4):** geradas pelo próprio processador.
- **Interrupções 5 a 7:** reservadas para uso futuro.
- **Interrupções externas e por software (8 a 15):** as externas são acionadas por pinos do barramento; as geradas por software são causadas pela execução da instrução `trap`.

| Nº | Endereço do quadro | Causa |
|:---:|:---:|---|
| 0 | `0000` | início de operação |
| 1 | `0008` | violação de segmento |
| 2 | `0010` | instrução privilegiada (executada fora do modo supervisor) |
| 3 | `0018` | divisão por zero |
| 4 | `0020` | instrução ilegal |
| 5 | `0028` | ausência de quadro (paginação) |
| 8–15 | `0040`–`0078` | `trap` correspondente ou interrupção externa |

### Vetor de interrupções

Cada interrupção possui um **quadro de desvio**: quatro valores de 16 bits (`ip`, `sp`, `cs`, `ds`) que são carregados nos respectivos registradores quando a interrupção ocorre. Se o valor de `ip` de um quadro for zero, aquela interrupção é ignorada quando ocorrer. O vetor de interrupções é armazenado a partir do endereço `0000` da memória física, ocupando 16 quadros × 4 palavras × 2 bytes = 128 bytes, região reservada e não disponível para outros usos.

A própria inicialização do processador se comporta como a interrupção número 0: ao ligar, o Mancha carrega `ip`, `sp`, `cs` e `ds` a partir do primeiro quadro do vetor, por isso valores coerentes devem estar previamente gravados a partir do endereço `0000`.

### Tratamento das interrupções

A cada interrupção, o processador entra automaticamente em modo supervisor e salva seus registradores na pilha (`r0` no topo, `s7` no fundo). Em seguida busca, no quadro de desvio correspondente, os novos valores de `ip`, `sp`, `cs` e `ds`. Os bits `S`, `I` e `D` de `sr` são ligados antes mesmo do salvamento, entrando automaticamente em modo supervisor, com entrada/saída habilitada e interrupções desabilitadas. O retorno do tratamento é feito com `rete`, que desempilha automaticamente todos os registradores salvos.

A instrução `trap IM₆` (com `IM₆` de 8 a 15) causa uma interrupção por software — é o mecanismo típico de **chamada de sistema**, usado por programas de usuário para solicitar serviços privilegiados ao sistema operacional.

---

## 8. Organização da Memória

Do ponto de vista de uma instrução em execução, um endereço de memória do Mancha possui 16 bits, permitindo o acesso a 65536 endereços diferentes (cada um correspondendo a um byte) — o **espaço de endereçamento** do processador.

Os acessos à memória se dividem em dois tipos: **acessos a instruções** (buscas, na posição dada por `ip`) e **acessos a dados** (na posição dada pelo modo de endereçamento). Em ambos os casos, o endereço usado pela instrução — chamado **endereço virtual** — sofre uma transformação (por segmentação ou por paginação) antes de chegar à memória física, resultando em um **endereço real**. Diz-se que o espaço de endereçamento de um processador com esse mecanismo é um espaço de **memória virtual**.

### Segmentação

A segmentação é o modo normal de operação do Mancha. A transformação de endereços é um simples deslocamento em relação ao início da memória, usando os registradores de supervisor `cs`/`cl` (base e tamanho do segmento de código) e `ds`/`dl` (base e tamanho do segmento de dados). O endereço real é calculado somando o endereço virtual com a base do segmento correspondente; um acesso a um endereço virtual maior que o tamanho do segmento é um erro de programação e causa uma **interrupção de violação de segmento**. Esse mecanismo permite manter diversos programas em memória sem necessidade de relocação, cada um limitado à sua própria região.

Quando o bit `S` de `sr` está ligado (modo supervisor), os limites de segmento são desconsiderados — um acesso fora dos limites não gera interrupção. É possível obter o mesmo efeito em modo usuário atribuindo o valor `0000` aos registradores de tamanho de segmento.

### Paginação

A paginação é um modo de memória virtual mais sofisticado, ativado pelo bit `P` de `sr`. Nesse modo o registrador de supervisor `s4` passa a ser chamado `pt` (ponteiro da tabela de páginas); `s5` a `s7` ficam sem função específica definida.

Os espaços de endereçamento (de instruções e de dados) são divididos em **páginas** de tamanho fixo de 512 bytes — 128 páginas em cada espaço, 256 páginas no total por programa. A memória real é dividida da mesma forma em **quadros** de 512 bytes. A associação entre páginas e quadros é feita por uma **tabela de páginas** com 256 entradas de 16 bits (o mesmo tamanho de um quadro de memória).

Cálculo do endereço real:

1. O **número da página** corresponde aos 7 bits mais significativos do endereço virtual (os 9 bits menos significativos formam o **deslocamento**, guardado para uso posterior). Em acessos a dados, soma-se 128 ao número da página (liga-se o oitavo bit) — páginas de instrução vão de 0 a 127 e páginas de dados de 128 a 255.
2. O número da página indexa a tabela de páginas, de onde se lê uma palavra de 16 bits. Os 3 bits mais significativos são os **bits de controle de acesso**; os 13 bits restantes são o **número do quadro** (até 8192 quadros, limitando a memória real a 4 megabytes).
3. O número do quadro é multiplicado por 512 (deslocado 9 bits à esquerda) para obter o endereço do primeiro byte do quadro na memória real; somando o deslocamento obtido no passo 1, chega-se ao **endereço real**.

Os bits de controle de acesso definem o tipo de entrada: `00` ausente (gera interrupção de ausência de quadro), `01` leitura, `10` escrita, `11` modificado (uma página de escrita é promovida a modificado no primeiro acesso de escrita). O bit menos significativo é o **bit de acesso**, ligado no primeiro acesso ao quadro — útil para o sistema operacional decidir quais quadros enviar para memória secundária em caso de falta de espaço (por exemplo, priorizando os quadros menos recentemente usados).

---

## 9. Resumo de Codificação

As tabelas completas de codificação binária de todas as instruções (usuário e privilegiadas), dos códigos de registrador, dos códigos de modo de endereçamento e dos limites de valores imediatos encontram-se no texto original (seção "Resumo do Processador Mancha"). Em suma:

- **Códigos de registrador (`rrr`/`sss`/`ddd`):** `000`=`r0`/`s0`(`sr`) … `101`=`r5`/`bp`/`s5`/`cl` … `111`=`r7`/`ip`/`s7`/`dl`.
- **Códigos de modo de endereçamento (`mmm`):** `000` direto, `001` indireto, `010` pós-incremento, `011` pré-decremento, `100` imediato, `101` absoluto, `110` deslocamento.
- **Valores imediatos:** `IM₆` (-32 a 31), `IM₁₀` (-512 a 511), `IMED` de 16 bits (-32768 a 32767 com sinal, 0 a 65535 sem sinal).

---

## 10. Ferramentas de Desenvolvimento

Para o processador Mancha foram desenvolvidos os seguintes programas:

### Montador (`mas`)

```
prompt% mas [opções] [-O listagem] [-o saída] [entrada]
```

Gera código objeto a partir de um arquivo fonte em linguagem de montagem Mancha. Reconhece as pseudo-instruções `.equ` (atribuição de símbolo), `.org` (muda endereço de carga corrente), `.db`/`.dw` (define bytes/palavras), `.ds` (define espaço não inicializado), `.ext`/`.pub` (declara símbolo externo/público) e `.text`/`.data`/`.bss` (seleciona segmento corrente). Um módulo fonte pode conter símbolos, rótulos e relocação, permitindo escrever módulos independentes que serão depois concatenados por um ligador.

### Ligador (`mld`)

```
prompt% mld [opções] -o saída entrada [...] [-l biblioteca]
```

Concatena módulos objeto, resolvendo referências externas entre eles, e gera um arquivo executável (ou objeto religável, com a opção `-i`).

### Simulador (`msim`)

```
prompt% msim [opções] inicial
```

Executa código objeto Mancha, simulando um computador completo: banco de memória de 1 MB, console (entrada/saída de caracteres), controlador de disco, relógio programável e controlador de interrupções externas. Suporta modo de depuração interativo (pontos de parada, inspeção e modificação de registradores e memória, execução passo a passo).

#### Dispositivos simulados (portas de E/S)

| Endereço | Função |
|---|---|
| `0001` | dados da console (entrada e saída de caracteres) |
| `0002` | estado da console (bit 0: caractere recebido; bit 1: pronto para enviar) |
| `0010`–`0012` | face, trilha e setor do disco a acessar |
| `0013` | registrador de operação do disco (escrita inicia a operação; leitura retorna o estado) |
| `0014`–`0015` | endereço de memória (DMA) para a operação de disco |
| `0020`–`0021` | contador do relógio |
| `0022`–`0023` | limite do relógio (gera interrupção quando o contador o alcança) |
| `0030` | controlador de interrupções externas (habilita/desabilita cada uma das interrupções 8 a 15) |

| Interrupção | Origem |
|:---:|---|
| 0–7 | internas (violação de segmento, instrução privilegiada, divisão por zero, etc.) |
| 8 | console |
| 9 | interface de disco |
| 10 | relógio |
| 11–15 | desocupadas |

### Utilitários complementares

- `msize`: lista o cabeçalho de um módulo objeto binário (tamanhos de cada área).
- `menc` / `mdec`: convertem, respectivamente, um arquivo no formato Mancha Mínimo para o formato Mancha completo e vice-versa.

### Formato do arquivo objeto

O formato de arquivo objeto/executável do Mancha é derivado do formato originalmente usado no Unix. É composto por: cabeçalho (assinatura `BACA` e tamanhos de cada área seguinte), área de carga do segmento de instruções, área de carga do segmento de dados inicializados, áreas de relocação (instruções e dados), área de símbolos públicos, área de símbolos externos e área de informações de depuração.

---

## 11. Exemplos em Assembly

Os dois exemplos a seguir são autocontidos (podem ser montados com `mas` e executados com `msim`) e mostram como combinar registradores, modos de endereçamento, pilha e interrupções na prática.

### 11.1 "Olá mundo!" no console

O console é acessado por duas portas de E/S (ver seção 10, "Dispositivos simulados"): `0001` (dados) e `0002` (estado, bit 0 = pronto para enviar). Escrever no console exige uma instrução privilegiada (`outb`), por isso este programa é executado diretamente pelo simulador em modo supervisor (como acontece com qualquer programa carregado no início da operação).

```asm
        .data
msg:    .db     "Ola mundo!", 10, 0     ; string terminada em zero (10 = '\n')

        .text
        .pub    main
main:
        ld      r1, msg          ; r1 = &msg (modo imediato: endereço como valor)

laco:
        ldb     r0, (r1+)        ; r0 = *r1 (byte); r1 += 1 (pós-incremento)
        cmp     r0, 0
        jmpc    eq, fim          ; byte nulo -> fim da string
        call    putc
        bra     laco

fim:
        halt

; ---------------------------------------------------------
; putc: imprime o caractere em r0 no console (espera a
; interface ficar pronta antes de enviar). Usa r2 e não
; altera r0, r1.
; ---------------------------------------------------------
putc:
        inb     r2, (0002)       ; r2 = estado da console
        andb    r2, 1            ; bit 0: pronto para enviar?
        jmpc    z, putc          ; ainda não -> espera (poll)
        outb    r0, (0001)       ; envia o caractere
        ret
```

### 11.2 Lendo e imprimindo o valor do relógio

Este exemplo estende a chamada de sistema `trap 12` (seção 7) para, além de devolver o valor do contador do relógio (porta `0020`, ver seção 10, "Dispositivos simulados") em `r0`, também imprimi-lo no console no formato `0xNNNN`. A conversão para hexadecimal usa a rotina auxiliar `imprime_hex`, que reaproveita `putc`.

```asm
; ---------------------------------------------------------
; imprime_hex: imprime r0 (16 bits) em hexadecimal (4 dígitos).
; Reaproveita putc; preserva r3 e r4 na pilha; destrói r0.
; ---------------------------------------------------------
imprime_hex:
        push    r3
        push    r4
        ldq     r4, 4            ; 4 dígitos hexadecimais a imprimir

prox_digito:
        ld      r3, r0
        shr     r3, 12           ; nibble mais significativo -> bits 0-3
        cmp     r3, 10
        jmpc    lt, decimal
        addb    r3, 55           ; 'A' - 10  (dígitos hexadecimais A-F)
        bra     converte

decimal:
        addb    r3, 48           ; '0'       (dígitos decimais 0-9)

converte:
        push    r0               ; preserva o valor ainda não impresso
        ld      r0, r3
        call    putc
        pop     r0
        shl     r0, 4            ; descarta o nibble já impresso
        addq    r4, -1
        cmp     r4, 0
        jmpc    nz, prox_digito

        pop     r4
        pop     r3
        ret

; ---------------------------------------------------------
; servico de sistema 12: lê e imprime o contador do relógio
; no formato "0xNNNN"; devolve o valor lido em r0 ao chamador
; (tratador registrado no quadro da trap 12 do vetor de
; interrupções — ver seção 7)
; ---------------------------------------------------------
le_relogio:
        in      r1, (0020)       ; r1 = contador do relógio (16 bits)
        ldq     r0, '0'
        call    putc
        ldq     r0, 'x'
        call    putc
        ld      r0, r1           ; r0 = valor a converter/imprimir
        call    imprime_hex      ; imprime os 4 dígitos hexadecimais
        ld      r0, r1           ; r0 = valor lido (retorno ao chamador)
        rete

; programa de usuário: pede a hora atual ao sistema operacional
        trap    12               ; imprime "0xNNNN" e devolve o valor em r0
```

`r1` é usado como "guarda-costas" do valor lido durante as chamadas a `putc`/`imprime_hex`, já que nenhuma das duas rotinas o utiliza — evitando a necessidade de empilhá-lo.

---

## 12. Mancha Mínimo

O **Mancha Mínimo** é uma versão simplificada do Mancha, usada tipicamente em disciplinas de organização de computadores (implementação de um pequeno processador com blocos funcionais digitais). Principais diferenças:

- Apenas quatro registradores: acumulador (`r0`), operando (`r1`), índice (`r2`) e `ip` (contador de programa).
- Apenas dois bits de estado: `Z` (zero) e `C` (*carry*), sem interrupções.
- Memória de apenas 1024 bytes; ao ser ativado, começa a executar a partir do endereço zero.
- Conjunto reduzido de instruções: `ldi` (carga imediata no acumulador), `ldo`/`ldx` (carrega operando/índice a partir do acumulador), `ldax`/`stax` (carga/armazenamento indexado), `in`/`out`, `jmpq`/`skip`/`nop`/`halt`, e as operações `add`, `and`, `or`, `xor`, `addx` entre acumulador e operando (ou índice).
- Arquivo objeto em formato texto ASCII, mais simples que o do Mancha completo.

O processador Mancha completo é capaz de executar todas as instruções do Mancha Mínimo, o que permite reaproveitar as mesmas ferramentas (montador, simulador) em ambas as disciplinas.

---

## Referências

O texto original cita formatos de arquivo objeto consagrados (COFF, ELF e a.out do Unix) como inspiração para o formato de arquivo objeto do Mancha, sem detalhar as referências bibliográficas completas.

---

*Documento preparado a partir do texto "Processador Mancha", de Marcelo Pasin (UFSM, 2004), com correções ortográficas e de digitação identificadas no original (por exemplo: "ferrramentas"→"ferramentas", "modo supervidor"→"modo supervisor", "instrção"→"instrução", "motador"→"montador", "sehuem"→"seguem", "limitesde"→"limites de", entre outras), reorganizado em formato de referência rápida.*
