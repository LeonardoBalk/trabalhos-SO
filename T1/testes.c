#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cpu.h"
#include "objeto.h"
#include "instrucao.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static uint32_t le32(disp_t *d, uint16_t porta)
{
  uint32_t valor = 0;
  for (int i = 0; i < 4; i++) valor = (valor << 8) | disp_le_byte(d, porta + i);
  return valor;
}

static void testa_dispositivo(void)
{
  mem_t *m = mem_cria();
  disp_t *d = disp_cria(m), *outro = disp_cria(m);
  disp_escreve_byte(d, 0x40, 1);
  for (int i = 0; i < 70000; i++) disp_tick(d);
  disp_escreve_byte(d, 0x40, 2);
  assert(le32(d, 0x41) == 70000);
  disp_tick(d);
  assert(le32(d, 0x41) == 70000);
  assert(le32(outro, 0x41) == 0);
  disp_escreve_byte(d, 0x40, 1);
#ifdef _WIN32
  Sleep(120);
#else
  usleep(120000);
#endif
  disp_escreve_byte(d, 0x40, 2);
  assert(le32(d, 0x45) >= 100 && le32(d, 0x45) < 2000);
  assert(le32(d, 0x41) == 0);
  disp_escreve_byte(d, 0x40, 1);
  disp_escreve_byte(d, 0x40, 2);
  assert(le32(d, 0x45) < 100);
  for (int i = 0; i < 100; i++) {
    (void)disp_le_byte(d, 0x50);
    uint8_t baixo = disp_le_byte(d, 0x51);
    disp_tick(d);
    assert(disp_le_byte(d, 0x51) == baixo);
  }
  disp_destroi(outro); disp_destroi(d); mem_destroi(m);
}

static int compara(const void *a, const void *b)
{
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return (x > y) - (x < y);
}

static void testa_programa(const char *arquivo, int modo, int repeticoes)
{
  mem_t *m = mem_cria();
  disp_t *d = disp_cria(m);
  cpu_t *cpu = cpu_cria(m, d);
  simbolo_t *simbolos = NULL;
  char erro[256];
  assert(obj_carrega(arquivo, m, &simbolos, erro, sizeof erro));
  obj_libera_simbolos(simbolos);
  cpu_liga(cpu);
  unsigned primeiro = 0;
  bool variou = false;
  for (int repeticao = 0; repeticao < repeticoes; repeticao++) {
    long inicio = -1;
    time_t limite = time(NULL) + 15;
    while (!cpu_parada(cpu)) {
      long passos = cpu_num_instrucoes(cpu);
      assert(passos < 200000000);
      if (passos % 10000 == 0) assert(time(NULL) < limite);
      uint16_t ip = cpu_r(cpu, 7);
      instr_decod_t inst;
      instrucao_decodifica(mem_le_palavra(m, ip), &inst);
      bool controle = inst.formato == FMT_REGISTRADOR && inst.codop == 13 &&
                      inst.mod == MOD_ABS && mem_le_palavra(m, ip + 2) == 0x40;
      uint16_t comando = controle ? cpu_r(cpu, inst.reg) : 0;
      if (comando == 1) inicio = passos;
      cpu_executa_1(cpu);
      if (comando == 2) {
        assert(inicio >= 0);
        assert(le32(d, 0x41) == (uint32_t)(passos - inicio));
      }
    }
    int tamanho;
    const char *saida = console_saida(d, &tamanho);
    char texto[4096];
    assert(tamanho > 0 && tamanho < (int)sizeof texto);
    memcpy(texto, saida, tamanho); texto[tamanho] = 0;
    if (modo == 0) {
      unsigned numero; int fim = 0;
      assert(sscanf(texto, "0x%x\n%n", &numero, &fim) == 1);
      assert(numero <= 65535 && fim == tamanho);
      if (repeticao == 0) primeiro = numero;
      else if (numero != primeiro) variou = true;
    } else if (modo == 1) {
      unsigned anterior_i = 0, anterior_s = 0;
      char *linha = strtok(texto, "\n");
      int linhas = 0;
      while (linha != NULL) {
        unsigned instrucoes, segundos;
        assert(sscanf(linha, "I=0x%x S=0x%x", &instrucoes, &segundos) == 2);
        assert(instrucoes > anterior_i && segundos > anterior_s);
        anterior_i = instrucoes; anterior_s = segundos;
        linhas++;
        linha = strtok(NULL, "\n");
      }
      assert(linhas >= 1 && anterior_s >= 5 && anterior_s <= 8);
      assert(le32(d, 0x45) >= 5000 && le32(d, 0x45) < 9000);
    } else {
      unsigned valores[20], esperado[10];
      char *cursor = texto;
      for (int i = 0; i < 20; i++) {
        cursor = strstr(cursor, "0x");
        assert(cursor && sscanf(cursor, "0x%x", &valores[i]) == 1);
        assert(valores[i] <= 65535);
        cursor += 6;
      }
      assert(strstr(cursor, "0x") == NULL);
      memcpy(esperado, valores, sizeof esperado);
      qsort(esperado, 10, sizeof *esperado, compara);
      for (int i = 0; i < 10; i++) assert(valores[i + 10] == esperado[i]);
    }
    console_limpa_saida(d);
    cpu_reinicia(cpu);
  }
  if (modo == 0) assert(variou);
  cpu_destroi(cpu); disp_destroi(d); mem_destroi(m);
}

int main(void)
{
  testa_dispositivo();
  testa_programa("t1.mob", 0, 16);
  testa_programa("contador.mob", 1, 2);
  testa_programa("leitura.mob", 2, 20);
  puts("OK: dispositivo, geracao automatica, tempo real, reinicios e ordenacao.");
  return 0;
}
