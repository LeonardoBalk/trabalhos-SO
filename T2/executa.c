#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "objeto.h"

#define MAX_ENTRADAS 32

typedef struct {
  long instante;
  const char *texto;
  bool feita;
} entrada_t;

static void poe_texto(disp_t *d, const char *s)
{
  for (; *s != '\0'; s++) {
    if (s[0] == '\\' && s[1] == 'n') {
      console_poe_entrada(d, '\n');
      s++;
    } else {
      console_poe_entrada(d, *s);
    }
  }
}

static void descarrega_console(disp_t *d)
{
  int n;
  const char *s = console_saida(d, &n);
  if (n > 0) {
    fwrite(s, 1, n, stdout);
    console_limpa_saida(d);
  }
}

int main(int argc, char *argv[])
{
  long max = 5000000;
  entrada_t entradas[MAX_ENTRADAS];
  int n_entradas = 0;
  const char *arquivo = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
      max = atol(argv[++i]);
    } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc && n_entradas < MAX_ENTRADAS) {
      char *arg = argv[++i];
      char *sep = strchr(arg, ':');
      if (sep == NULL) {
        entradas[n_entradas] = (entrada_t){ 0, arg, false };
      } else {
        *sep = '\0';
        entradas[n_entradas] = (entrada_t){ atol(arg), sep + 1, false };
      }
      n_entradas++;
    } else {
      arquivo = argv[i];
    }
  }
  if (arquivo == NULL) {
    fprintf(stderr, "uso: %s [-n max] [-e instrucao:texto]... arquivo.mob\n", argv[0]);
    return 2;
  }

  mem_t *m = mem_cria();
  disp_t *d = disp_cria(m);
  cpu_t *cpu = cpu_cria(m, d);
  simbolo_t *simbolos = NULL;
  char erro[256];
  if (!obj_carrega(arquivo, m, &simbolos, erro, sizeof erro)) {
    fprintf(stderr, "erro ao carregar '%s': %s\n", arquivo, erro);
    return 1;
  }
  obj_libera_simbolos(simbolos);
  cpu_liga(cpu);

  while (!cpu_parada(cpu) && cpu_num_instrucoes(cpu) < max) {
    for (int i = 0; i < n_entradas; i++) {
      if (!entradas[i].feita && cpu_num_instrucoes(cpu) >= entradas[i].instante) {
        poe_texto(d, entradas[i].texto);
        entradas[i].feita = true;
      }
    }
    cpu_executa_1(cpu);
    descarrega_console(d);
  }
  descarrega_console(d);

  fprintf(stderr, "\n[%s após %ld instruções]\n",
          cpu_parada(cpu) ? "parou" : "limite atingido", cpu_num_instrucoes(cpu));
  cpu_destroi(cpu);
  disp_destroi(d);
  mem_destroi(m);
  return 0;
}
