#ifndef SISTEMA_H
#define SISTEMA_H

int so_le(void);
int so_escreve(int c);
int so_cria_proc(char *nome);
int so_mata_proc(int pid);
int so_espera_proc(int pid);

int putchar(int c);
int getchar(void);
void puts(char *s);
void print_str(char *s);
void print_int(int v);
void print_hex(int v);

#endif
