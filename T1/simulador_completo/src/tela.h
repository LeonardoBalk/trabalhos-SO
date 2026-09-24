// tela.h -- interface de operador em curses, no mesmo estilo do
// simulador do Mancha Mínimo (mesmos comandos, mesmo layout por seções
// com um cursor de linha), estendida para mostrar os registradores de
// supervisor, o modo de operação, as flags completas e os dispositivos
// (console/disco/relógio/controlador de interrupções) do Mancha completo.

#ifndef TELA_H
#define TELA_H

#include <stdbool.h>
#include "cpu.h"
#include "memoria.h"
#include "dispositivos.h"

void tela_inicializa(void);
void tela_finaliza(void);
int tela_le_tecla(void);

void tela_desenha(cpu_t *cpu, mem_t *mem, disp_t *disp,
                   const char *arquivo, bool rodando, int velocidade,
                   const char *mensagem, const char *linha_comando);

#endif
