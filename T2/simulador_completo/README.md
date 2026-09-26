# Simulador do processador Mancha completo

Simulador do processador **Mancha completo** (Marcelo Pasin, 2004), descrito
em `../mancha.pdf` (secções 2 a 13). Inclui um montador para a linguagem de
montagem do Mancha e um simulador com interface em texto (`ncurses`) que
mostra o estado da CPU durante a execução — na mesma interface e com os
mesmos comandos de operador do
[simulador do Mancha Mínimo](../simulador/README.md), estendida para mostrar
os registradores de supervisor, o modo de operação, as flags completas e os
dispositivos simulados.

## Arquitetura simulada

- 16 registradores de 16 bits: 8 de usuário (`r0`-`r4`, `bp`, `sp`, `ip`) e 8
  de supervisor (`sr`, `s1`-`s3`, `cs`/`pt`, `cl`, `ds`, `dl`), só acessíveis
  em modo supervisor.
- Registrador de estado `sr` com 8 bits de condição: `S` (supervisor), `P`
  (paginação), `I` (entrada/saída permitida), `D` (interrupções desabilitadas),
  `N`, `O`, `Z`, `C`. O PDF não define a posição exata desses bits dentro de
  `sr` (só a ordem visual na figura 1); este projeto usa bit15=S, bit14=P,
  bit13=I, bit12=D, bit3=N, bit2=O, bit1=Z, bit0=C — uma escolha interna e
  consistente, sem efeito sobre a semântica descrita no texto.
- 7 modos de endereçamento (direto, indireto, pós-incremento, pré-decremento,
  imediato, absoluto, deslocamento) e 4 formatos de instrução (registrador,
  especial, condicional, rápido) — ver secções 4 e 6 do PDF.
- ~40 instruções (secções 5, 6 e 9), incluindo aritmética com `mul`/`div`,
  pilha (`push`/`pop`/`call`/`ret`), interrupções por software (`trap`/`rete`)
  e instruções privilegiadas.
- Memória real de 1MB, endereçada em 20 bits; cada instrução em execução
  enxerga um espaço de endereçamento **virtual** de 16 bits, traduzido para
  endereço real por **segmentação** (modo normal) ou **paginação** (secção
  8), conforme o bit `P` de `sr`.
- Vetor de interrupções de 16 quadros (128 bytes, a partir do endereço 0000
  da memória real) — secção 7. O próprio início de operação do processador é
  tratado como a interrupção 0.
- Dispositivos simulados (secção 13, tabela 17): console (porta 0001/0002,
  interrupção 8), disco com 2 unidades (portas 0010-0015, interrupção 9),
  relógio (portas 0020-0023, interrupção 10) e controlador de máscara de
  interrupções externas (porta 0030).

## Simplificações assumidas

**Montador sem ligador separado** (mesma decisão e mesma justificativa do
projeto do Mancha Mínimo): este montador resolve todos os símbolos para
endereços absolutos já na montagem — não há marcas de relocação binárias
nem um programa `mld` separado. Os três segmentos (`.text`/`.data`/`.bss`)
compartilham um único contador de endereço de carga; eles só marcam, no
arquivo objeto, se cada byte pertence ao segmento de código ou de dados.
Vários arquivos `.asm` passados numa só chamada do montador são tratados
como um único módulo, o que permite usar `.ext`/`.pub` entre eles mesmo sem
ligador de verdade. O formato do arquivo objeto (`.mob`, ver
`src/objeto.h`) também é uma versão simplificada, em texto, do formato
binário com relocação da secção 12 do PDF.

**Disco com tempo simulado, não assíncrono de verdade**: uma operação de
disco fica "ocupada" por um pequeno número fixo de passos de simulação antes
de completar e gerar a interrupção 9 — só para tornar visível na interface
que a operação está em andamento, não uma emulação de tempo real de disco.

**Relógio ligado ao passo de simulação**: a expressão "a cada pulso de
relógio do processador" (secção 13) foi interpretada como "a cada instrução
executada" (não há um clock de hardware real para basear isso). Ao bater no
limite, o contador volta a zero automaticamente, para servir como
temporizador periódico; o PDF não diz se isso deveria acontecer.

**Portas de entrada/saída de 8 bits, sempre**: os dispositivos simulados
(tabela 17) são registrados individuais de 8 bits em portas próprias —
inclusive o relógio, cujo valor de 16 bits é exposto como dois registradores
de 8 bits em portas adjacentes, não como uma porta de 16 bits (a secção
"Relógio" do texto corrido e a tabela 17 do PDF descrevem isso de forma
ligeiramente inconsistente entre si; este projeto segue a tabela, que é a
referência mais específica). Por isso `in`/`out` (largura palavra) acessam a
mesma porta de 8 bits que `inb`/`outb`, só estendendo o valor para 16 bits;
eles nunca leem/escrevem a porta seguinte.

**Todos os registradores são salvos/restaurados em toda interrupção**: a
secção 7 do PDF ("Tratamento das Interrupções") diz que **todos** os
registradores são salvos na pilha a cada interrupção (r0 no topo, s7 no
fundo), mas a descrição da instrução `trap`, na secção 5, menciona só
`ip`, `sp` e os registradores de supervisor sendo salvos — uma
inconsistência interna do texto. Este projeto segue a secção 7 (mais geral e
mais detalhada) para **todo** tipo de interrupção, inclusive `trap`: os 16
registradores (`r0`-`r7`, `s0`-`s7`) são empilhados na entrada e
desempilhados por `rete`. Na prática, isso significa que uma rotina de
tratamento não consegue devolver um valor para o código interrompido através
de um registrador de uso geral — só através de memória.

## Build

```sh
make            # compila bin/montador, bin/simulador, e monta os exemplos
```

Requer `gcc` e a biblioteca `ncurses` (`libncurses-dev` no Debian/Ubuntu).

## Uso

```sh
./bin/montador exemplos/soma.asm -o exemplos/soma.mob
./bin/simulador exemplos/soma.mob [-D imagem_disco0] [-D imagem_disco1]
```

(`make` já monta os `.asm` de `exemplos/` automaticamente.) A opção `-D`
monta um arquivo de imagem de disco na unidade correspondente (0 ou 1); se
o arquivo não existir, ele é criado (vazio) e gravado de volta ao sair do
simulador.

O ponto de entrada da execução vem do **quadro de desvio 0** (endereço
0000 da memória, secção 7 do PDF): ele deve estar corretamente montado no
programa (normalmente via `.org 0` seguido de `.dw ip_inicial, sp_inicial,
cs_inicial, ds_inicial`), já que é assim que o processador "acorda". Se ele
não estiver definido, o simulador mostra um aviso e o `ip` permanece 0000.

Requer terminal com pelo menos 90 colunas x 36 linhas.

### Comandos do operador

Os mesmos do simulador do Mancha Mínimo:

| comando   | efeito |
|-----------|--------|
| `E<texto>`| põe `<texto>` (seguido de um `\n`) na fila de entrada da console |
| `Z`       | limpa a tela de saída da console |
| `D<n>`    | muda a velocidade da simulação, `n` de 0 (mais rápida) a 9 (mais lenta) |
| `1`       | executa uma instrução (ou atende uma interrupção pendente) e pára |
| `C`       | continua a execução |
| `P`       | pára a execução |
| `R`       | reinicia a CPU (registradores em 0, refaz o boot do quadro 0) |
| `F`       | termina o simulador |

## Exemplos (`exemplos/`)

- **`soma.asm`**: calcula `par1 + par2` e guarda em `resultado`, depois pára.
  Roda inteiramente em modo supervisor (o modo em que o processador começa
  após o quadro de boot). Bom para conferir carga/armazenamento com modo de
  endereçamento absoluto.
- **`conta.asm`**: imprime `"5 4 3 2 1 "` na console usando a porta de dados
  (0001) diretamente com `out`, e `jmpc`/`cmp` para o laço.
- **`eco.asm`**: espera caracteres digitados (comando de operador `E`) e ecoa
  cada um de volta, usando espera ativa no bit de estado da console (porta
  0002). Não termina sozinho.
- **`protecao.asm`**: demonstra a mudança de modo supervisor/usuário e a
  proteção por segmentação. O código supervisor configura os limites de
  segmento do usuário (`cs`/`cl`/`ds`/`dl`), monta manualmente na pilha um
  quadro como o que uma interrupção deixaria, e usa `rete` para "retornar"
  para código de usuário. O código de usuário tenta executar `halt`, uma
  instrução privilegiada — isso causa a interrupção 2 (instrução
  privilegiada), que devolve o controle ao supervisor, que imprime `"PRIV"`
  e pára de verdade.
- **`paginacao.asm`**: demonstra a memória virtual paginada. A tabela de
  páginas é escrita diretamente pelo montador (não construída em tempo de
  execução): mapeia a página de instruções 0 no quadro 0 (identidade) e a
  página de dados 128 (endereços virtuais de dados 0000-01FF) no quadro 16,
  fora da área do código. O comentário no início do arquivo explica também
  um detalhe interessante descoberto ao escrever este exemplo: os
  registradores `cs` (base do segmento de código, em modo segmentação) e
  `pt` (ponteiro da tabela de páginas, em modo paginação) são o **mesmo**
  registrador físico (`s4`), o que exige cuidado especial na transição de um
  modo para o outro.

## Validação

Cada instrução do montador foi conferida byte a byte contra o exemplo
montado da figura 13/14 do `mancha.pdf` (`push bp` = `055E`, `ld bp,sp` =
`0146`, `sub sp,22` = `0DA0 0016`, todos nos mesmos deslocamentos relativos
do dump do PDF). Os cinco exemplos foram testados tanto com um executor de
CPU sem interface (só lógica) quanto interativamente via `tmux`, simulando
um operador real digitando comandos:

- `soma.mob`: `r0` = 15 (=5+10), pára após 5 instruções. ✓
- `conta.mob`: console recebe exatamente `"5 4 3 2 1 "`, pára após 64
  instruções. ✓
- `eco.mob`: caracteres enviados via comando `E` do operador (ex.:
  `"Ola mundo"`) aparecem exatamente iguais na saída da console. ✓
- `protecao.mob`: entra em modo usuário via `rete`, a tentativa de `halt`
  em modo usuário causa a interrupção 2 (`instrucao privilegiada`
  aparece como último evento na tela), o tratador imprime `"PRIV"` e pára de
  verdade em modo supervisor. ✓
- `paginacao.mob`: escreve e lê de volta corretamente através da tabela de
  páginas (`r3`=1 = sucesso), com a tela mostrando `P=1`, modo "paginacao" e
  o registrador `pt` corretamente identificado. ✓

Durante a validação foram encontrados e corrigidos dois problemas reais de
implementação (não apenas do exemplo em teste):

1. Instruções sem operandos do formato condicional (`trap`, `rete`, `halt`,
   `di`, `ei`) estavam sendo montadas com o campo `COND` em `1111` em vez de
   `0000`, por causa de um valor sentinela `-1` do montador sendo mascarado
   incorretamente ao empacotar a palavra.
2. `in`/`out` de largura palavra espalhavam o valor por duas portas
   adjacentes (como o relógio de 16 bits), o que quebrava a console (cujas
   portas de dados e de estado são registros de 8 bits independentes,
   **não** duas metades de um registro de 16 bits) — corrigido conforme a
   simplificação de portas de 8 bits descrita acima.

## Organização do código

```
src/
  instrucao.h/.c    codificação/decodificação das instruções, dos modos de
                     endereçamento e das condições de desvio (usado por
                     montador e simulador -- fonte única da verdade)
  memoria.h/.c       memória real de 1MB, leitura/escrita de bytes e palavras
  dispositivos.h/.c  console, disco, relógio e controlador de interrupções
  cpu.h/.c           registradores, tradução de endereços (segmentação e
                     paginação), interrupções e execução de uma instrução
                     por vez
  objeto.h/.c        leitura/escrita do formato de arquivo objeto (.mob)
  montador.c         montador (assembler): .asm -> .mob
  tela.h/.c          desenho da tela (curses)
  main.c             laço principal do simulador e comandos do operador
exemplos/
  *.asm              programas de exemplo em linguagem de montagem
```
