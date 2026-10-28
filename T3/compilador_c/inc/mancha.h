// mancha.h -- "biblioteca padrão" mínima para o subconjunto de C do mcc.
//
// C puro (o subconjunto suportado por este compilador) não tem nenhuma
// forma de fazer entrada/saída sozinho -- não existe uma "chamada de
// sistema" no Mancha, só instruções de porta (in/out) que só fazem
// sentido em modo supervisor. mancha_in/mancha_out/mancha_halt são
// implementadas em rt/runtime.asm com essas instruções; as demais funções
// deste cabeçalho (putchar, getchar, puts, print_int, print_hex) são
// escritas em C mesmo (rt/biblioteca.c) e compiladas pelo mcc, usando só
// as três primitivas abaixo.
//
// Este compilador não suporta funções variádicas (sem "...", portanto sem
// um printf(fmt, ...) de verdade) -- use print_int/print_hex/puts.

#ifndef MANCHA_H
#define MANCHA_H

// acesso direto a uma porta de entrada/saída (mancha.pdf, secção 13)
void mancha_out(int porta, int valor);
int mancha_in(int porta);

// para a simulação (equivalente a "halt": só funciona em modo supervisor,
// que é como todo programa compilado pelo mcc roda)
void mancha_halt(void);

// console (porta de dados 0001, porta de estado 0002, mancha.pdf tabela 17)
int putchar(int c);      // escreve um caractere, devolve c
int getchar(void);        // espera (ocupado) e lê um caractere
void puts(char *s);        // escreve uma string seguida de '\n'
void print_int(int v);      // escreve um inteiro decimal (com sinal)
void print_hex(int v);       // escreve 4 dígitos hexadecimais (maiúsculos)

#endif
