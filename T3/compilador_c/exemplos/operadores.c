// operadores.c -- operadores bit a bit e de deslocamento, módulo,
// ternário, curto-circuito de "&&"/"||", ponteiros e aritmética de
// ponteiros, atribuições compostas e "++"/"--" prefixo/posfixo.

#include "mancha.h"

int main(void)
{
    int a;
    int b;
    int v[5];
    int *p;
    int i;

    a = 13;
    b = 5;
    print_int(a & b); putchar(' ');
    print_int(a | b); putchar(' ');
    print_int(a ^ b); putchar(' ');
    print_int(~a); putchar(' ');
    print_int(a << 2); putchar(' ');
    print_int(a >> 1); putchar(' ');
    print_int(a % b); putchar('\n');

    print_int((a > b) ? a : b); putchar(' ');
    print_int((a < b) && (b > 0)); putchar(' ');
    print_int((a < b) || (b > 0)); putchar('\n');

    i = 0;
    while (i < 5) {
        v[i] = i * i;
        i = i + 1;
    }
    p = v;
    i = 0;
    while (i < 5) {
        print_int(*p);
        putchar(' ');
        p++;
        i = i + 1;
    }
    putchar('\n');

    p = &v[2];
    print_int(*(p + 2));
    putchar(' ');
    print_int(*(p - 1));
    putchar('\n');

    a = 10;
    a += 5; print_int(a); putchar(' ');
    a -= 3; print_int(a); putchar(' ');
    a *= 2; print_int(a); putchar(' ');
    a /= 4; print_int(a); putchar(' ');
    a %= 4; print_int(a); putchar('\n');

    i = 3;
    print_int(i++); putchar(' ');
    print_int(i); putchar(' ');
    print_int(++i); putchar(' ');
    print_int(i--); putchar(' ');
    print_int(i); putchar('\n');

    return 0;
}
