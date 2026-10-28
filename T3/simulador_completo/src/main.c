// main.c -- simulador do processador Mancha completo (mancha.pdf, secções
// 2 a 13), com a mesma interface de operador do simulador do Mancha
// Mínimo: tela em curses mostrando o estado da CPU, e uma linha de
// comando com os mesmos comandos de operador.
//
// uso: ./simulador arquivo.mob [-D imagem_disco0] [-D imagem_disco1]
//
// comandos do operador (digitados na linha de comando, seguidos de enter):
//   E<texto>  poe <texto> na fila de entrada da console
//   Z         limpa a tela de saida da console
//   D<n>      muda a velocidade da simulacao (n de 0 a 9; 0 e' a mais rapida)
//   1         executa uma instrucao (ou atende uma interrupcao) e para
//   C         continua a execucao (continua)
//   P         para a execucao
//   R         reinicia a CPU (registradores em 0, refaz o boot do quadro 0)
//   F         termina o simulador

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curses.h>

#include "memoria.h"
#include "dispositivos.h"
#include "cpu.h"
#include "objeto.h"
#include "tela.h"

#define ALTURA_MINIMA 36
#define LARGURA_MINIMA 90
#define TAM_CMD 256

int main(int argc, char *argv[])
{
  const char *arquivo = NULL;
  const char *discos[DISCO_UNIDADES] = {0};
  int n_discos = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
      if (n_discos < DISCO_UNIDADES) discos[n_discos++] = argv[++i];
      else i++;
    } else if (arquivo == NULL) {
      arquivo = argv[i];
    }
  }

  if (arquivo == NULL) {
    fprintf(stderr, "uso: %s arquivo.mob [-D imagem_disco0] [-D imagem_disco1]\n", argv[0]);
    fprintf(stderr, "  (monte um .asm com o montador antes: ./bin/montador entrada.asm)\n");
    return 1;
  }

  mem_t *mem = mem_cria();
  disp_t *disp = disp_cria(mem);
  cpu_t *cpu = cpu_cria(mem, disp);

  for (int i = 0; i < n_discos; i++) {
    char erro[256];
    if (!disp_monta_disco(disp, i, discos[i], erro, sizeof(erro))) {
      fprintf(stderr, "erro ao montar disco %d: %s\n", i, erro);
      return 1;
    }
  }

  char erro[256];
  simbolo_t *simbolos = NULL;
  if (!obj_carrega(arquivo, mem, &simbolos, erro, sizeof(erro))) {
    fprintf(stderr, "erro ao carregar '%s': %s\n", arquivo, erro);
    return 1;
  }
  obj_libera_simbolos(simbolos);

  cpu_liga(cpu);

  tela_inicializa();

  if (LINES < ALTURA_MINIMA || COLS < LARGURA_MINIMA) {
    tela_finaliza();
    fprintf(stderr, "terminal pequeno demais (%dx%d); use pelo menos %dx%d\n",
            COLS, LINES, LARGURA_MINIMA, ALTURA_MINIMA);
    return 1;
  }

  bool rodando = false;
  int velocidade = 5;
  char mensagem[256] = "PARADO. Digite C para rodar, 1 para um passo, F para sair.";
  char cmd[TAM_CMD] = "";
  int cmd_len = 0;

  int elapsed_ms = 0;
  const int tick_ms = 15;

  if (cpu_r(cpu, 7) == 0)
    snprintf(mensagem, sizeof(mensagem),
             "aviso: quadro 0 (boot) nao definido no programa -- ip permanece 0000");

  bool fim = false;
  while (!fim) {
    int c;
    while ((c = tela_le_tecla()) != ERR) {
      if (c == '\n' || c == '\r' || c == KEY_ENTER) {
        cmd[cmd_len] = '\0';

        if (cmd_len > 0) {
          char letra = cmd[0];
          char *resto = cmd + 1;

          switch (letra) {
            case 'E': case 'e':
              for (char *p = resto; *p != '\0'; p++) console_poe_entrada(disp, *p);
              console_poe_entrada(disp, '\n');
              snprintf(mensagem, sizeof(mensagem), "entrada adicionada: \"%s\"", resto);
              break;

            case 'Z': case 'z':
              console_limpa_saida(disp);
              snprintf(mensagem, sizeof(mensagem), "tela da console limpa");
              break;

            case 'D': case 'd':
              if (resto[0] >= '0' && resto[0] <= '9') {
                velocidade = resto[0] - '0';
                snprintf(mensagem, sizeof(mensagem), "velocidade alterada para D%d", velocidade);
              } else {
                snprintf(mensagem, sizeof(mensagem), "uso: D<0-9>");
              }
              break;

            case '1':
              if (cpu_parada(cpu)) {
                snprintf(mensagem, sizeof(mensagem), "CPU parada, R reinicia");
              } else {
                cpu_executa_1(cpu);
                rodando = false;
                snprintf(mensagem, sizeof(mensagem), "executado 1 passo");
              }
              break;

            case 'C': case 'c':
              if (cpu_parada(cpu)) {
                snprintf(mensagem, sizeof(mensagem), "CPU parada, R reinicia");
              } else {
                rodando = true;
                elapsed_ms = 0;
                snprintf(mensagem, sizeof(mensagem), "rodando...");
              }
              break;

            case 'P': case 'p':
              rodando = false;
              snprintf(mensagem, sizeof(mensagem), "parado pelo operador");
              break;

            case 'R': case 'r':
              cpu_reinicia(cpu);
              rodando = false;
              snprintf(mensagem, sizeof(mensagem), "CPU reiniciada (ip=%04X)", cpu_r(cpu, 7));
              break;

            case 'F': case 'f':
              fim = true;
              break;

            default:
              snprintf(mensagem, sizeof(mensagem), "comando desconhecido: '%.100s'", cmd);
          }
        }

        cmd_len = 0;
        cmd[0] = '\0';
      } else if (c == KEY_BACKSPACE || c == 127 || c == 8) {
        if (cmd_len > 0) cmd[--cmd_len] = '\0';
      } else if (c >= 32 && c < 127 && cmd_len < TAM_CMD - 1) {
        cmd[cmd_len++] = (char)c;
        cmd[cmd_len] = '\0';
      }
    }

    if (rodando && !cpu_parada(cpu)) {
      if (velocidade == 0) {
        for (int i = 0; i < 2000 && !cpu_parada(cpu); i++) cpu_executa_1(cpu);
      } else {
        elapsed_ms += tick_ms;
        if (elapsed_ms >= velocidade * 100) {
          cpu_executa_1(cpu);
          elapsed_ms = 0;
        }
      }
      if (cpu_parada(cpu)) {
        rodando = false;
        snprintf(mensagem, sizeof(mensagem), "CPU parada (halt) apos %ld instrucoes",
                  cpu_num_instrucoes(cpu));
      }
    }

    tela_desenha(cpu, mem, disp, arquivo, rodando, velocidade, mensagem, cmd);

    usleep(tick_ms * 1000);
  }

  tela_finaliza();

  cpu_destroi(cpu);
  disp_destroi(disp);
  mem_destroi(mem);

  return 0;
}
