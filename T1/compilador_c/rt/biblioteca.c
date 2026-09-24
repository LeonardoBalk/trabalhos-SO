// biblioteca.c -- funções de E/S de mais alto nível, escritas no próprio
// subconjunto de C compilado pelo mcc (ver inc/mancha.h), usando só as
// três primitivas de acesso a porta implementadas em runtime.asm.

#include "mancha.h"

int putchar(int c)
{
    mancha_out(1, c);
    return c;
}

int getchar(void)
{
    int st;
    st = mancha_in(2);
    while ((st & 2) == 0) {
        st = mancha_in(2);
    }
    return mancha_in(1);
}

void puts(char *s)
{
    while (*s != 0) {
        putchar(*s);
        s = s + 1;
    }
    putchar(10);
}

void print_int(int v)
{
    char buf[8];
    int i;
    int neg;

    i = 0;
    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    if (v == 0) {
        buf[0] = '0';
        i = 1;
    }
    while (v > 0) {
        buf[i] = (v % 10) + '0';
        i = i + 1;
        v = v / 10;
    }
    if (neg) {
        putchar('-');
    }
    while (i > 0) {
        i = i - 1;
        putchar(buf[i]);
    }
}

void print_hex(int v)
{
    char *digitos;
    int i;
    int nibble;

    digitos = "0123456789ABCDEF";
    i = 12;
    while (i >= 0) {
        nibble = (v >> i) & 15;
        putchar(digitos[nibble]);
        i = i - 4;
    }
}
