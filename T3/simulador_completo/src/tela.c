// tela.c -- desenho da tela do simulador do Mancha completo, no mesmo
// estilo do simulador do Mancha Mínimo (curses, layout com cursor de
// linha "y" avançando seção a seção -- ver mancha_minimo/src/tela.c).

#include "tela.h"
#include "instrucao.h"

#include <curses.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>

#define COR_STATUS   1
#define COR_TITULO   2
#define COR_MEM_IP   3
#define COR_MENSAGEM 4
#define COR_SUPERV   5

#define CONSOLE_LINHAS 6
#define MEM_LINHAS     6
#define MEM_PALAVRAS_POR_LINHA 8

void tela_inicializa(void)
{
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  timeout(0);
  keypad(stdscr, TRUE);
  curs_set(0);

  if (has_colors()) {
    start_color();
    init_pair(COR_STATUS,   COLOR_BLACK, COLOR_CYAN);
    init_pair(COR_TITULO,   COLOR_BLACK, COLOR_WHITE);
    init_pair(COR_MEM_IP,   COLOR_BLACK, COLOR_YELLOW);
    init_pair(COR_MENSAGEM, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COR_SUPERV,   COLOR_BLACK, COLOR_GREEN);
  }
}

void tela_finaliza(void) { endwin(); }
int tela_le_tecla(void) { return getch(); }

// ---------------------------------------------------------------- console

static int desenha_console(int y0, int col0, int largura, int n_linhas, disp_t *disp)
{
  mvhline(y0, col0, ACS_HLINE, largura);
  mvprintw(y0, col0 + 2, " console (saida) ");

  int tam;
  const char *saida = console_saida(disp, &tam);

  int l = y0 + n_linhas;
  int linha_fim = tam;

  while (l >= y0 + 1 && linha_fim > 0) {
    int linha_ini = linha_fim;
    int c = 0;
    while (linha_ini > 0 && saida[linha_ini - 1] != '\n' && c < largura) { linha_ini--; c++; }
    move(l, col0);
    clrtoeol();
    for (int j = linha_ini; j < linha_fim; j++)
      if (saida[j] != '\n') addch((unsigned char)saida[j]);
    if (linha_ini > 0 && saida[linha_ini - 1] == '\n') linha_ini--;
    linha_fim = linha_ini;
    l--;
  }
  while (l >= y0 + 1) { move(l, col0); clrtoeol(); l--; }

  return y0 + n_linhas + 1;
}

// ------------------------------------------------------------- memória

static int desenha_memoria(int y0, int col0, cpu_t *cpu, mem_t *mem)
{
  uint16_t ip = cpu_r(cpu, 7);
  int metade = (MEM_LINHAS * MEM_PALAVRAS_POR_LINHA * 2) / 2;
  int base = (ip - metade) & ~0xF;
  if (base < 0) base = 0;
  if (base > 0xFFFF - MEM_LINHAS * MEM_PALAVRAS_POR_LINHA * 2)
    base = 0xFFFF - MEM_LINHAS * MEM_PALAVRAS_POR_LINHA * 2;

  mvprintw(y0, col0, "memoria (janela de enderecos virtuais em volta do ip; traduzidos p/ segmento de codigo):");
  for (int i = 0; i < MEM_LINHAS; i++) {
    uint16_t end = (uint16_t)(base + i * MEM_PALAVRAS_POR_LINHA * 2);
    move(y0 + 1 + i, col0);
    printw("%04X: ", end);
    for (int j = 0; j < MEM_PALAVRAS_POR_LINHA; j++) {
      uint16_t e = (uint16_t)(end + j * 2);
      uint32_t real = cpu_traduz_exibicao(cpu, e, false);
      uint16_t palavra = mem_le_palavra(mem, real);
      bool eh_ip = (e == ip);
      if (eh_ip) attron(COLOR_PAIR(COR_MEM_IP) | A_BOLD);
      printw("%04X ", palavra);
      if (eh_ip) attroff(COLOR_PAIR(COR_MEM_IP) | A_BOLD);
    }
  }
  return y0 + 1 + MEM_LINHAS + 1;
}

// --------------------------------------------------------------- geral

void tela_desenha(cpu_t *cpu, mem_t *mem, disp_t *disp,
                   const char *arquivo, bool rodando, int velocidade,
                   const char *mensagem, const char *linha_comando)
{
  erase();

  int altura, largura;
  getmaxyx(stdscr, altura, largura);

  int y = 0;

  attron(COLOR_PAIR(COR_TITULO) | A_BOLD);
  mvhline(y, 0, ' ', largura);
  mvprintw(y, 1, "Simulador Mancha completo  --  %s", arquivo);
  attroff(COLOR_PAIR(COR_TITULO) | A_BOLD);
  y += 2;

  y = desenha_console(y, 1, largura - 2, CONSOLE_LINHAS, disp);
  y++;

  bool supervisor = cpu_modo_supervisor(cpu);
  bool paginacao = cpu_paginacao_ativa(cpu);

  attron(COLOR_PAIR(COR_STATUS));
  mvhline(y, 0, ' ', largura);
  mvprintw(y, 1, "%s  modo=%s  %s  D%d  instr=%ld  evento: %s",
            rodando ? "RODANDO" : "PARADO",
            supervisor ? "SUP" : "USR",
            paginacao ? "paginacao" : "segmentacao",
            velocidade, cpu_num_instrucoes(cpu), cpu_ultimo_evento(cpu));
  attroff(COLOR_PAIR(COR_STATUS));
  y += 2;

  // registradores de usuário
  mvprintw(y, 1, "r0=%04X   r1=%04X   r2=%04X   r3=%04X   r4=%04X",
            cpu_r(cpu,0), cpu_r(cpu,1), cpu_r(cpu,2), cpu_r(cpu,3), cpu_r(cpu,4));
  y++;
  mvprintw(y, 1, "bp=%04X   sp=%04X   ip=%04X",
            cpu_r(cpu,5), cpu_r(cpu,6), cpu_r(cpu,7));
  y += 2;

  // registradores de supervisor
  attron(COLOR_PAIR(COR_SUPERV));
  mvprintw(y, 1, "sr=%04X   s1=%04X   s2=%04X   s3=%04X",
            cpu_s(cpu,0), cpu_s(cpu,1), cpu_s(cpu,2), cpu_s(cpu,3));
  y++;
  if (paginacao)
    mvprintw(y, 1, "pt=%04X (ponteiro da tabela de paginas)   s5=%04X   s6=%04X   s7=%04X",
              cpu_s(cpu,4), cpu_s(cpu,5), cpu_s(cpu,6), cpu_s(cpu,7));
  else
    mvprintw(y, 1, "cs=%04X   cl=%04X   ds=%04X   dl=%04X",
              cpu_s(cpu,4), cpu_s(cpu,5), cpu_s(cpu,6), cpu_s(cpu,7));
  attroff(COLOR_PAIR(COR_SUPERV));
  y += 2;

  // flags
  mvprintw(y, 1, "flags: S=%d P=%d I=%d D=%d   N=%d O=%d Z=%d C=%d%s",
            cpu_bit(cpu, SR_S), cpu_bit(cpu, SR_P), cpu_bit(cpu, SR_I), cpu_bit(cpu, SR_D),
            cpu_bit(cpu, SR_N), cpu_bit(cpu, SR_O), cpu_bit(cpu, SR_Z), cpu_bit(cpu, SR_C),
            cpu_parada(cpu) ? "     ** CPU PARADA (halt) **" : "");
  y++;

  // dispositivos
  bool ocupado, erro_disco;
  int ticks;
  disp_estado_disco(disp, 0, &ocupado, &erro_disco, &ticks);
  mvprintw(y, 1, "relogio: contador=%04X limite=%04X   disco0: %s%s   mascara interrupcoes=%02X",
            disp_relogio_contador(disp), disp_relogio_limite(disp),
            ocupado ? "ocupado" : "livre", erro_disco ? " (erro)" : "",
            disp_mascara_interrupcoes(disp));
  y += 2;

  // próxima instrução
  char desmontado[64];
  cpu_desmonta_proxima(cpu, desmontado, sizeof(desmontado));
  mvprintw(y, 1, "proxima instrucao em %04X: %s", cpu_r(cpu,7), desmontado);
  y += 2;

  y = desenha_memoria(y, 1, cpu, mem);
  y++;

  attron(COLOR_PAIR(COR_MENSAGEM));
  mvhline(y, 1, ' ', largura - 2);
  if (mensagem != NULL && mensagem[0] != '\0') mvprintw(y, 1, "%s", mensagem);
  attroff(COLOR_PAIR(COR_MENSAGEM));

  int lin_cmd = altura - 2;
  mvhline(lin_cmd - 1, 0, ACS_HLINE, largura);
  mvprintw(lin_cmd, 1, "comando> %s", linha_comando);
  mvprintw(altura - 1, 1,
    "E<texto> entra  Z limpa  D<n> velocidade  1 passo  C continua  P para  R reinicia  F fim");

  refresh();
}
