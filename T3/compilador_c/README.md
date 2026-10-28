# mcc -- um compilador C para o Mancha completo

`mcc` compila um subconjunto de C ("C mínimo, mais structs e
pré-processador, sem ponto flutuante, sem otimizações") para a linguagem
de montagem do [simulador do Mancha completo](../simulador_completo/),
usando o montador daquele projeto como backend (`mcc` só gera texto
assembly; quem transforma isso em `.mob` é
`../simulador_completo/bin/montador`).

```
programa.c --[mcc]--> programa.asm --[montador, junto com rt/*.asm]--> programa.mob --[simulador]--> executa
```

## Por que este subconjunto

O objetivo foi cobrir o que dá mais retorno por esforço para programas
didáticos de arquitetura de computadores: aritmética, controle de fluxo,
funções (inclusive recursivas), ponteiros, arrays e structs -- o
suficiente para escrever coisas como listas ligadas, matrizes e pequenas
bibliotecas -- sem entrar em terreno caro de implementar (ponto
flutuante, otimizações de código) que não muda o que dá para *ensinar*
com o compilador. Cada simplificação abaixo está documentada com o motivo.

## Sumário

1. [Build e uso](#build-e-uso)
2. [Tipos](#tipos)
3. [Declarações e inicializadores](#declarações-e-inicializadores)
4. [Structs](#structs)
5. [Ponteiros e arrays](#ponteiros-e-arrays)
6. [Operadores](#operadores)
7. [Comandos](#comandos)
8. [Funções](#funções)
9. [Ponteiros de função](#ponteiros-de-função)
10. [Pré-processador](#pré-processador)
11. [Biblioteca padrão (`mancha.h`)](#biblioteca-padrão-manchah)
12. [O que não é suportado](#o-que-não-é-suportado)
13. [Convenção de chamada e organização da memória](#convenção-de-chamada-e-organização-da-memória)
14. [Exemplos (`exemplos/`)](#exemplos-exemplos)
15. [Validação](#validação)
16. [Organização do código](#organização-do-código)

## Build e uso

```sh
make            # compila bin/mcc, rt/biblioteca.asm e monta exemplos/*.c em .mob
```

Requer `gcc` e que `../simulador_completo` já esteja compilado (`make
all` naquele diretório produz `bin/montador`, usado por este Makefile).

Para compilar um programa próprio:

```sh
bin/mcc -Iinc meuprograma.c -o meuprograma.asm
../simulador_completo/bin/montador rt/runtime.asm rt/biblioteca.asm meuprograma.asm -o meuprograma.mob
../simulador_completo/bin/simulador meuprograma.mob
```

`rt/runtime.asm` (ponto de entrada e as três primitivas de porta) e
`rt/biblioteca.asm` (gerado a partir de `rt/biblioteca.c` pelo próprio
`mcc` -- `putchar`/`getchar`/`puts`/`print_int`/`print_hex`) têm que ser
montados **junto** com o `.asm` do programa: o montador do Mancha
completo trata vários arquivos passados numa só chamada como um módulo
só (resolve todos os símbolos entre eles), então isso funciona como uma
"ligação" simples, sem precisar de um ligador de verdade -- ver
`../simulador_completo/README.md`.

O programa precisa de uma função `main` (`int main(void)` ou `void
main(void)`, sem argumentos -- não há linha de comando neste ambiente).
Fora isso, `mcc` não impõe nenhum nome especial.

## Tipos

| tipo | tamanho | observação |
|---|---|---|
| `char` | 1 byte | **sem sinal** (0 a 255) -- ver nota abaixo |
| `int` | 2 bytes (16 bits) | com sinal |
| `void` | -- | só como tipo de retorno ou em `f(void)` |
| `T *` | 2 bytes | ponteiro para qualquer tipo, inclusive `T **` |
| `T nome[N]` | `N * sizeof(T)` | array de tamanho fixo, `N` constante |
| `struct Nome` | soma dos membros | sem preenchimento/alinhamento (ver [Structs](#structs)) |
| `RET (*nome)(...)` | 2 bytes | ponteiro de função (ver [Ponteiros de função](#ponteiros-de-função)) |

Não existem `float`/`double`, `long`/`short`, `unsigned`/`signed`,
`enum`, `union` nem `typedef` -- ver a lista completa em
[O que não é suportado](#o-que-não-é-suportado).

`char` é sempre **sem sinal** neste compilador (opção de projeto: em C
padrão isso é definido pela implementação; aqui, considerando que o Mancha
não tem uma instrução de extensão de sinal e a maioria dos usos de `char`
são caracteres/bytes de dados, sem sinal é a escolha mais previsível).

`sizeof` é suportado com um tipo (`sizeof(int)`, `sizeof(struct Ponto)`,
`sizeof(char*)`) ou uma expressão (`sizeof(x)`, `sizeof(*p)`) -- sempre
entre parênteses, e nunca avalia a expressão (assim como em C de verdade).
`sizeof` de um ponteiro de função só funciona na forma de expressão
(`sizeof(fp)`, `fp` já declarado) -- a forma de tipo abstrato
(`sizeof(int (*)(int))`) não é suportada, mesma restrição de
[Ponteiros de função](#ponteiros-de-função) sobre casts.

## Declarações e inicializadores

```c
int x;                    // global: zerada
int y = 10;                 // global: inicializada com uma constante
char letra = 'A';
int tabela[5] = {1, 2, 3};    // os elementos que faltam viram 0
char nome[] = "abc";           // ok mas o TAMANHO precisa ser dado (char nome[4] = "abc";
                                 // "abc" tem 4 bytes contando o '\0') -- mcc não
                                 // deduz o tamanho do array a partir do inicializador

int soma(int a, int b)
{
    int total;              // local: pode ser declarada em qualquer ponto de um
    total = a + b;            // bloco (não só no início, ao contrário do C89)
    return total;
}
```

Inicializadores de variáveis **globais** têm que ser constantes em tempo
de compilação (literais inteiros/caracteres, com `-`/`~`/`!` unários, ou
uma string entre aspas para `char*`/`char[]`) -- sem chamadas de função
nem variáveis. Inicializadores de variáveis **locais** podem ser
qualquer expressão, avaliada em tempo de execução:

```c
int dobro(int n) { return n * 2; }

int main(void)
{
    int x = dobro(21);   // ok: inicializador local, roda em tempo de execução
    ...
}
```

Vários declaradores por linha são permitidos (`int a, b, c;`, `int a =
1, b = 2;`), mas uma função nunca divide linha com outra declaração.

## Structs

```c
struct Ponto {
    int x;
    int y;
};

struct Retangulo {
    struct Ponto topo_esq;
    struct Ponto baixo_dir;    // structs podem estar aninhadas
};

struct No {
    int valor;
    struct No *prox;            // ponteiro para o próprio tipo: sempre ok,
};                                 // mesmo antes da struct estar "completa"
```

- Sem preenchimento/alinhamento: os membros ficam exatamente um depois do
  outro (`sizeof(struct Ponto) == 4`, dois `int`s de 2 bytes cada, sem
  buracos). Isso simplifica o cálculo de layout; o custo é que acessos
  desalinhados são um pouco mais lentos no hardware real -- irrelevante
  aqui, já que o Mancha não tem essa penalidade (memória é sempre
  endereçada por byte).
- Acesso a membro: `s.campo` (struct direta) ou `p->campo` (ponteiro para
  struct) -- os dois funcionam em qualquer combinação de aninhamento
  (`r.topo_esq.x`, `p->campo.sub.valor`, etc.).
- **Atribuição de struct** (`s1 = s2;`) é suportada: copia byte a byte.
- **Não suportado**: passar ou devolver uma struct *por valor* em uma
  função (nem como parâmetro nem como retorno) -- sempre passe um
  ponteiro (`void f(struct Ponto *p)`), que é como o exemplo
  `structs.c` faz. `union`, `enum`, bit-fields e structs anônimas também
  não são suportados.
- Uma definição `struct Nome { ... };` tem que estar sozinha numa
  declaração (não dá para escrever `struct Nome { ... } variavel;` na
  mesma linha -- declare a variável numa linha separada).

## Ponteiros e arrays

```c
int v[5];
int *p;

p = v;                 // um array "decai" para ponteiro do seu primeiro elemento
p = &v[2];               // ou tira o endereço de um elemento específico
*p = 10;                   // v[2] agora vale 10
p++;                         // p aponta para v[3] (aritmética de ponteiro escala
                               // pelo sizeof do tipo apontado)
print_int(*(p + 1));           // v[4]

int matriz[3][4];              // array multidimensional (linha-maior)
matriz[1][2] = 7;

struct No *cabeca;              // ponteiro para struct
cabeca = 0;                       // "0" é o ponteiro nulo (não existe uma
                                    // palavra-chave NULL neste subconjunto)
```

`sizeof`, `&`, `*`, `[]`, `->`, `.`, aritmética de ponteiro (`p + n`, `p -
n`, `p1 - p2`) e comparação de ponteiros (`==`, `!=`, `<`, etc.) são
suportados. Não há alocação dinâmica (`malloc`/`free`) -- estruturas de
dados como listas ligadas usam um "pool" estático (array global), como em
`exemplos/lista.c`.

## Operadores

Da maior para a menor precedência (mesma ordem de C):

| categoria | operadores | associatividade |
|---|---|---|
| pós-fixo | `()` `[]` `.` `->` `++` `--` | esquerda |
| unário | `+` `-` `!` `~` `*` `&` `++` `--` `sizeof` `(tipo)` | direita |
| multiplicativo | `*` `/` `%` | esquerda |
| aditivo | `+` `-` | esquerda |
| deslocamento | `<<` `>>` | esquerda |
| relacional | `<` `<=` `>` `>=` | esquerda |
| igualdade | `==` `!=` | esquerda |
| E bit a bit | `&` | esquerda |
| OU-exclusivo bit a bit | `^` | esquerda |
| OU bit a bit | `\|` | esquerda |
| E lógico | `&&` (com curto-circuito) | esquerda |
| OU lógico | `\|\|` (com curto-circuito) | esquerda |
| condicional | `?:` | direita |
| atribuição | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | direita |

Observações específicas desta implementação:

- **`char` é sem sinal** (ver [Tipos](#tipos)), então `>>` num `char` ou
  num `int` sempre desloca preenchendo com zero à esquerda (deslocamento
  lógico) -- o Mancha não tem uma instrução de deslocamento aritmético;
  em C padrão o comportamento de `>>` num tipo com sinal e valor negativo
  já é definido pela implementação, então isso está dentro do permitido
  pela linguagem, só vale saber que aqui é sempre lógico.
- Não existe o operador vírgula (`a, b` como expressão) fora da lista de
  argumentos de uma chamada -- em `for (;;)`, cada cláusula aceita só uma
  expressão.
- Comparações e aritmética são sempre feitas em 16 bits com sinal (exceto
  os operandos `char`, que são zero-estendidos antes).

## Comandos

```c
if (cond) { ... } else if (cond2) { ... } else { ... }

while (cond) { ... }

do { ... } while (cond);

for (int i = 0; i < 10; i++) { ... }     // inicialização pode ser uma declaração
for (;;) { ... }                            // as três cláusulas podem ser vazias

break;      // só dentro de while/do/for
continue;    // idem

return;          // função void
return expr;      // função com retorno
```

`switch`/`case` e `goto`/rótulos não são suportados -- use `if`/`else
if` (o exemplo `fatorial.c` e outros mostram o estilo esperado).

## Funções

```c
int soma(int a, int b);              // protótipo (declaração sem corpo):
                                        // permite chamar antes de definir,
                                        // ou declarar uma função de outro
                                        // arquivo .c montado junto (ver
                                        // "convenção de chamada" abaixo)

int soma(int a, int b)                // definição
{
    return a + b;
}

void nada(void) { }                     // "void" nos parênteses = zero parâmetros

int fatorial(int n)                      // recursão funciona normalmente
{
    if (n <= 1) return 1;
    return n * fatorial(n - 1);
}
```

- Até 16 parâmetros por função.
- Os nomes dos parâmetros podem faltar num protótipo (`int f(int, int);`),
  mas são obrigatórios na definição.
- Uma função sem `return` explícito no fim do corpo simplesmente retorna
  (com um valor de retorno indefinido, se não for `void` -- como em C).
- Não há funções variádicas (sem `...`) -- por isso não existe um
  `printf(fmt, ...)` de verdade; veja `print_int`/`print_hex`/`puts` na
  biblioteca padrão. Ponteiros para função são suportados -- ver a
  próxima seção.
- Todo símbolo (função ou variável global) tem ligação externa (não há
  `static` para restringir um nome a um arquivo) -- evite repetir nomes
  de função/variável global entre arquivos `.c` diferentes que serão
  montados juntos.

## Ponteiros de função

```c
int soma(int a, int b) { return a + b; }
int subtrai(int a, int b) { return a - b; }

int (*op)(int, int);     // variável: ponteiro para função(int,int) retornando int
op = soma;                  // o nome de uma função, usado sem "()", vale o
                              // ENDEREÇO dela (mesma ideia de um array
                              // decaindo pra ponteiro fora de "[]")
print_int(op(3, 4));           // chamada indireta: 7
op = subtrai;
print_int(op(10, 4));            // 6
print_int((*op)(10, 4));           // "(*op)(...)" é equivalente a "op(...)"
                                      // (desreferenciar um ponteiro de função
                                      // não faz nada -- ele já É o endereço)
```

**Tabela de despacho** (array de ponteiros de função, indexado por um
código de operação) -- o mesmo padrão usado para um vetor de
interrupções ou uma tabela de chamadas de sistema:

```c
int multiplica(int a, int b) { return a * b; }

int (*tabela[3])(int, int) = { soma, subtrai, multiplica };

for (i = 0; i < 3; i++) {
    print_int(tabela[i](6, 2));     // 8 4 12
}
```

**Callback** (ponteiro de função como parâmetro) e **ponteiro de função
como membro de struct** (uma "tabela de comandos") também são
suportados, e usam exatamente a mesma sintaxe de declarador:

```c
void aplica(int (*f)(int, int), int a, int b) { print_int(f(a, b)); }
aplica(soma, 20, 5);       // 25

struct Comando { char letra; int (*executa)(int, int); };
struct Comando comandos[2] = { { 'S', soma }, { 'D', subtrai } };
print_int(comandos[0].executa(8, 3));     // 11
```

Veja `exemplos/ponteiros_funcao.c` para um exemplo com os quatro padrões
juntos.

### O que é suportado

- Declarar uma variável, parâmetro, membro de struct ou array de
  ponteiros de função: `TIPO (*nome)(parâmetros)` e
  `TIPO (*nome[N])(parâmetros)`.
- Atribuir o nome de uma função (sem `()`) a uma variável de ponteiro de
  função, a um elemento de array, a um membro de struct, ou passá-lo
  como argumento -- o nome "decai" para o endereço da função, do mesmo
  jeito que um array decai para ponteiro do primeiro elemento.
- Chamar através de qualquer expressão de tipo "ponteiro de função":
  uma variável (`fp(...)`), `(*fp)(...)`, um elemento de array
  (`tabela[i](...)`), um membro de struct (`s.executa(...)` ou
  `p->executa(...)`) -- todas geram uma chamada indireta (`call rN`, o
  endereço calculado em um registrador), diferente de uma chamada direta
  pelo nome da função (`minhafuncao(...)`, que gera `call rotulo`, sem
  indireção).
- Inicializador de variável **global** de ponteiro de função (ou array/
  struct contendo um), com o nome de uma função já declarada -- inclusive
  já com um protótipo só, cujo corpo é definido mais adiante no arquivo:

  ```c
  void processa(void);           // protótipo: já basta pra usar o nome abaixo
  void (*gancho)(void) = processa;  // a ordem que importa é a do PROTÓTIPO,
                                       // não a de onde "processa" é definida
  void processa(void) { ... }         // pode vir depois, sem problema
  ```
- Comparação (`fp == 0`, `fp != outra_funcao`) e a checagem de aridade
  (número de argumentos) numa chamada indireta, feita a partir do tipo
  declarado do ponteiro (`int (*)(int,int)` exige exatamente 2
  argumentos, por exemplo).

### O que não é suportado

- **`&minhafuncao`** (tirar o endereço de uma função explicitamente com
  `&`): use o nome sozinho (`minhafuncao`), que já vale o endereço --
  `&` sobre uma função dá erro de compilação.
- **Ponteiro de ponteiro de função** (`int (**fp)(int)`) e **função
  retornando ponteiro de função** (`int (*escolhe(int))(int)`): só um
  nível de indireção é suportado. Sem `typedef` (que o mcc também não
  suporta), essas formas já são pouco práticas de escrever mesmo em C de
  verdade.
- **Cast para tipo de ponteiro de função** (`(int (*)(int)) endereco`):
  atribua o valor a uma variável do tipo certo primeiro
  (`int (*fp)(int); fp = ...; fp(x);`) em vez de um cast inline.
- Inicializador de ponteiro de função em variável **global** com algo
  além do nome direto de uma função (ou `0`) -- mesma restrição que
  qualquer outro inicializador global, que precisa ser resolvível em
  tempo de montagem (ver [Declarações e inicializadores](#declarações-e-inicializadores)).
  Em variável **local**, qualquer expressão vale, exatamente como para
  os outros tipos escalares.

## Pré-processador

```c
#define TAMANHO 10
#define DOBRO(x) ((x) * 2)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "outroarquivo.h"        // busca: pasta do arquivo atual, depois -I
#include <mancha.h>               // busca: só nas pastas -I

#ifndef CABECALHO_H
#define CABECALHO_H
...
#endif

#if defined(DEPURACAO) && !defined(RELEASE)
...
#elif OUTRA_CONDICAO
...
#else
...
#endif

#undef TAMANHO
```

- Macros de objeto (`#define NOME valor`) e de função (`#define
  NOME(a,b) corpo`), com expansão recursiva (uma macro pode usar outra),
  mas **sem** variádicos (`...`/`__VA_ARGS__`) e sem os operadores `#`
  (stringize) e `##` (paste).
- `#include "arquivo"` e `#include <arquivo>`, `-I` na linha de comando
  do `mcc` para diretórios extras (o Makefile sempre passa `-Iinc`, onde
  fica `mancha.h`).
- `#ifdef`/`#ifndef`/`#else`/`#endif`/`#undef` completos.
- `#if`/`#elif`: suporta `defined(X)` (ou `defined X`), literais
  inteiros, `!`, `&&`, `\|\|`, `==`, `!=`, `<`, `<=`, `>`, `>=` e
  parênteses -- o suficiente para guardas de inclusão e chaveamento de
  código por macro numérica, mas não é o avaliador completo do C.
- `#error` interrompe a compilação com uma mensagem. `#pragma` e `#line`
  são aceitos e ignorados.
- Comentários `//` e `/* */`, e continuação de linha com `\` no final,
  são tratados durante o pré-processamento (funcionam em qualquer lugar,
  não só dentro de diretivas).

## Biblioteca padrão (`mancha.h`)

C puro não tem nenhuma forma de fazer entrada/saída sozinho -- o Mancha
não tem uma "chamada de sistema", só instruções de porta (`in`/`out`),
que este subconjunto de C não expõe como sintaxe. `inc/mancha.h` declara
um punhado de funções para isso:

```c
void mancha_out(int porta, int valor);   // acesso direto a uma porta (mancha.pdf, secção 13)
int  mancha_in(int porta);
void mancha_halt(void);                    // equivalente a "halt" (só funciona em modo
                                              // supervisor, que é como todo programa
                                              // compilado pelo mcc roda)

int  putchar(int c);       // escreve um caractere na console, devolve c
int  getchar(void);         // espera (ocupado) e lê um caractere da console
void puts(char *s);          // escreve uma string seguida de '\n'
void print_int(int v);        // escreve um inteiro decimal (com sinal)
void print_hex(int v);         // escreve 4 dígitos hexadecimais maiúsculos
```

`mancha_out`/`mancha_in`/`mancha_halt` são implementadas em
`rt/runtime.asm` (só elas realmente precisam das instruções `in`/`out`/
`halt`); `putchar`/`getchar`/`puts`/`print_int`/`print_hex` são escritas
no próprio subconjunto de C, em `rt/biblioteca.c`, e compiladas pelo
`mcc` -- ou seja, a biblioteca "se compila com o próprio compilador",
o que serve também como um teste de verdade do compilador em código não
trivial (laços, ponteiros, aritmética).

Não existe um `printf` de verdade (precisaria de funções variádicas, não
suportadas) -- combine `print_int`/`print_hex`/`puts`/`putchar`.

## O que não é suportado

Para deixar bem explícito (além do que já foi mencionado seção por
seção acima):

- `float`, `double` -- o Mancha não tem instruções de ponto flutuante;
  emular em software é caro e foi propositalmente deixado de fora do
  escopo deste compilador (ver o documento de estimativa de esforço).
- `long`, `short`, `unsigned`, `signed` -- só `int` (16 bits com sinal) e
  `char` (8 bits sem sinal).
- `enum`, `union`, bit-fields, structs/unions anônimas.
- `typedef`.
- `static`, `extern`, `const`, `volatile`, `register` (nenhum
  qualificador de armazenamento ou de tipo).
- Funções variádicas (`...`). Ponteiros para função **são** suportados
  (ver [Ponteiros de função](#ponteiros-de-função)), com as restrições
  próprias listadas naquela seção (sem `&funcao`, sem múltiplos níveis
  de indireção, sem cast para tipo de ponteiro de função).
- `switch`/`case`, `goto`, rótulos.
- Operador vírgula fora de lista de argumentos.
- Passagem/retorno de struct por valor (use ponteiro).
- Inferência de tamanho de array a partir do inicializador (`int v[] =
  {1,2,3};` não é suportado -- escreva `int v[3] = {1,2,3};`).
- `malloc`/`free`/alocação dinâmica.
- `#` e `##` no pré-processador, macros variádicas, `#if` com expressões
  arbitrariamente complexas.
- Múltiplos arquivos-fonte com nomes de função/variável global repetidos
  (sem `static`, todo símbolo é "global" de verdade quando os `.asm`
  são montados juntos).

## Convenção de chamada e organização da memória

Documentado aqui porque é útil tanto para quem for ler o código gerado
(`mcc programa.c -o programa.asm` e abrir o `.asm`) quanto para quem for
escrever mais funções de runtime em assembly (como `rt/runtime.asm`
faz):

- **Registradores**: `r0`-`r4` são de uso livre (não preservados entre
  chamadas -- "caller-saved"); `bp` é o ponteiro de quadro; `sp` é o
  topo da pilha; `ip` é o contador de instruções. Todo o código gerado
  roda em **modo supervisor** o tempo todo (é o modo em que o
  processador liga, ver `../simulador_completo/README.md`) -- não há
  mudança para modo usuário nem uso dos registradores de supervisor.
- **Passagem de parâmetros**: empilhados da **direita para a esquerda**
  antes do `call`, sempre como uma palavra de 16 bits cada (mesmo os
  `char`) -- ou seja, para `f(a, b, c)`, empilha-se `c`, depois `b`,
  depois `a`, e só então executa `call`. O primeiro parâmetro declarado
  fica em `(bp+4)`, o segundo em `(bp+6)`, e assim por diante.
- **Prólogo/epílogo**: toda função começa com `push bp` / `ld bp, sp` /
  `sub sp, N` (`N` = bytes de variáveis locais, calculado por uma
  passada de "pré-varredura" antes de gerar o código de verdade -- ver
  comentário no topo de `src/parser.c`) e termina com `ld sp, bp` / `pop
  bp` / `ret` a cada `return` (não só no final).
- **Valor de retorno**: em `r0`.
- **Quem desempilha os argumentos**: quem chama (`add sp, N` logo depois
  do `call`), não a função chamada.
- **Chamada indireta** (através de um ponteiro de função): os argumentos
  são empilhados do mesmo jeito, mas em vez de `call rotulo` o
  compilador avalia a expressão do ponteiro por último (depois de todos
  os argumentos já empilhados) e emite `call r0` -- o Mancha aceita
  qualquer modo de endereçamento como alvo de `call`, inclusive um
  registrador (ver `mancha.pdf`, formato especial da instrução).
- **Variáveis locais**: todo o quadro da função é alocado de uma vez no
  prólogo (o Mancha não tem uma instrução de "alocar N bytes agora"), em
  `(bp-2)`, `(bp-4)`, etc., na ordem de declaração -- mesmo variáveis
  declaradas dentro de um `if`/`while` ganham espaço reservado desde o
  prólogo (só a *visibilidade* do nome é limitada ao bloco).
- **Strings e globais**: `.data`, com um rótulo por variável global e um
  rótulo por literal de string (`_str<algo>_N`); nada é colocado em
  `.bss` -- mesmo globais sem inicializador são escritas explicitamente
  como zeros, para não ter que lidar com a granularidade de palavra do
  `.ds` do montador quando o tamanho é ímpar.
- **Ponto de entrada**: `rt/runtime.asm` define o quadro de desvio 0
  (`.org 0`, `.dw _start, 0xF000, 0, 0`) e uma pilha inicial em `0xF000`;
  `_start` só faz `call _f_main` seguido de `halt`.
- **Sem otimizações, de propósito**: cada subexpressão é avaliada
  passando por `r0`, com valores intermediários salvos na pilha real
  quando preciso (nunca em registradores "reservados"), e cada
  comparação recalcula as flags do zero -- o código gerado é
  deliberadamente direto/repetitivo, não o mais compacto possível. Isso
  foi uma escolha, não uma limitação: o objetivo é um compilador simples
  de entender e de verificar, não um gerador de código eficiente.

  Uma pegadinha real da ISA que valeu a pena documentar (custou um bug
  real durante o desenvolvimento -- ver [Validação](#validação)): no
  Mancha, `ld`/`ldb`/`pop` (e `ldq`/`in`/`swap`) **alteram as flags
  N/Z** com base no valor carregado -- não são "transparentes" como se
  poderia supor. Por isso o gerador de código nunca insere nenhuma
  instrução entre um `cmp` e o `jmpc` que o usa. E `ldb` só substitui o
  byte **baixo** do registrador de destino (o byte alto fica com o que
  já estava lá) -- por isso toda carga de um `char` é seguida de `and
  r0, 255` para garantir um valor de 16 bits corretamente zero-estendido.

## Exemplos (`exemplos/`)

- **`ola.c`**: primeiro programa, E/S básica (`puts`/`print_int`/
  `print_hex`) e aritmética.
- **`fatorial.c`**: funções recursivas (fatorial e fibonacci).
- **`structs.c`**: structs aninhadas, ponteiro para struct (`->`),
  struct passada por ponteiro para uma função, array global de structs.
- **`preprocessador.c`**: macros de objeto/função (inclusive aninhadas)
  e compilação condicional (`#ifndef`/`#define`, `#if`, `#ifdef`).
- **`operadores.c`**: bit a bit, deslocamento, módulo, ternário,
  curto-circuito de `&&`/`\|\|`, ponteiros e aritmética de ponteiro,
  atribuições compostas, `++`/`--` prefixo e posfixo.
- **`lista.c`**: struct autorreferenciada (lista ligada) usando um pool
  estático de nós (sem `malloc`), e um array multidimensional (matriz).
- **`eco.c`**: `getchar()`/espera ocupada -- interativo, use o comando de
  operador `E<texto>` do simulador para digitar uma linha.
- **`ponteiros_funcao.c`**: ponteiro de função (variável, tabela de
  despacho, callback, membro de struct) -- ver
  [Ponteiros de função](#ponteiros-de-função).

## Validação

Cada exemplo foi testado de duas formas: com um executor de CPU sem
interface (`.mob` rodado direto pelo núcleo do simulador, checando o
conteúdo exato da console) e interativamente via `tmux`, simulando um
operador real digitando comandos no simulador de verdade
(`../simulador_completo/bin/simulador`).

- `ola.mob`: console = `"Ola, Mancha!\n14\n1234\n"`, pára após 1240
  instruções. ✓
- `fatorial.mob`: `fatorial(0..7)` = `1 1 2 6 24 120 720 5040`,
  `fibonacci(0..7)` = `0 1 1 2 3 5 8 13`. ✓
- `structs.mob`: área do retângulo = 12, ponto deslocado = `(10,-3)`,
  array de structs = `(0,0) (2,3) (4,6)`. ✓
- `preprocessador.mob`: array preenchido por macro = `1 3 5 7 9`,
  `MAX(3,7)=7`, `MAX(DOBRO(10),5)=20`, ramo `#if` correto escolhido. ✓
- `operadores.mob`: todos os resultados de bit a bit/deslocamento/
  módulo/ternário/curto-circuito/ponteiro/atribuição composta/
  incremento conferidos à mão. ✓
- `lista.mob`: lista ligada construída por inserção na cabeça = `1 2
  3`, soma = 6; matriz 3×4 preenchida e lida em ordem linha-maior. ✓
- `eco.mob`: testado tanto com entrada injetada no executor sem
  interface quanto interativamente via `tmux` (comando `E` do
  simulador) -- eco de `"Ola mundo"` seguido de `"!"`, pára após 884
  instruções, batendo exatamente entre os dois métodos. ✓
- `ponteiros_funcao.mob`: variável de ponteiro de função (`soma(3,4)=7`,
  `subtrai(10,4)=6`), tabela de despacho (`8 4 12`), callback
  (`aplica(soma,20,5)=25`, `aplica(multiplica,20,5)=100`), struct com
  ponteiro de função inicializada com chaves aninhadas dentro de um
  array (`S=11 D=5 M=24`) -- todos conferidos à mão, testado no
  executor sem interface e interativamente no simulador real. ✓

Dois bugs reais de implementação foram encontrados e corrigidos durante a
validação (documentados com mais detalhe no comentário de
`materializa_booleano` e `carrega`, em `src/parser.c`):

1. O gerador de código inseria um `ld r0, 0` entre o `cmp` e o `jmpc` ao
   materializar um valor booleano (resultado de `==`, `<`, `!`, etc.) --
   como `ld` também altera as flags N/Z no Mancha, isso descartava o
   resultado da comparação antes do desvio condicional ler.
2. `ldb` só substitui o byte baixo do registrador de destino (não zera o
   byte alto), então comparações de 16 bits sobre um `char` recém-lido
   (como o teste de fim de string `*s != 0`) podiam dar resultado errado
   por causa de lixo no byte alto -- corrigido mascarando com `and r0,
   255` depois de toda carga de um byte.

Nenhum bug novo apareceu ao adicionar suporte a ponteiros de função --
o design (representar um ponteiro de função como um `T_PONTEIRO`
apontando para um novo `T_FUNCAO`, em vez de inventar um mecanismo
separado) fez a maior parte da máquina de tipos existente (decaimento de
array, indexação, membro de struct, inicializador global) funcionar sem
alterações, então a superfície de código realmente nova (parsing do
declarador `(*nome)(...)`, detecção de chamada direta/indireta,
`ld r0, rótulo` para decair uma função nua) era pequena o bastante para
sair certa da primeira vez nos testes.

## Organização do código

```
src/
  pre.h/.c        pré-processador (comentários, #include, #define, #if...)
  lexer.h/.c      tokenizador (opera sobre a saída já expandida do pré-processador)
  tipos.h/.c       sistema de tipos (int/char/void/ponteiro/array/struct/
                    ponteiro de função) e tabela de structs
  simbolos.h/.c     tabela de símbolos globais e pilha de escopos locais
  parser.c          o compilador propriamente dito: parser recursivo-descendente
                      que gera pequenas árvores por expressão (resolve lvalue/
                      rvalue em atribuições e o "não avaliar" de sizeof(expr)) e
                      emite assembly diretamente ao processar declarações/comandos
  main.c            driver: lê o(s) argumento(s), roda o pré-processador, o lexer
                      e o compilador, escreve o .asm de saída
inc/
  mancha.h          "biblioteca padrão" (protótipos)
rt/
  runtime.asm       ponto de entrada (_start) e mancha_out/mancha_in/mancha_halt
  biblioteca.c       putchar/getchar/puts/print_int/print_hex, escritas no próprio
                       subconjunto de C e compiladas pelo mcc (rt/biblioteca.asm)
exemplos/
  *.c               programas de exemplo em C
```
