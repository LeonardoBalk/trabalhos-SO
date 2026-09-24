#include "dispositivos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DISCO_TICKS_OPERACAO 20
#define DISCO_TAM_UNIDADE (DISCO_FACES * DISCO_TRILHAS * DISCO_SETORES * DISCO_TAM_SETOR)

typedef struct {
  uint8_t *dados;
  char *caminho; // NULL se não montado (unidade fica sempre presente, vazia)

  uint8_t face, trilha, setor;
  uint16_t endereco;
  bool ocupado;
  bool erro;
  int ticks_restantes;
  uint8_t operacao_pendente; // 00 leitura, 01 escrita, 10 identifica
  uint8_t unidade_pendente;
} disco_unidade_t;

struct dispositivos {
  mem_t *mem;

  // console
  char entrada[CONSOLE_ENTRADA_MAX];
  int entrada_inicio, entrada_fim, entrada_n;
  char saida[CONSOLE_SAIDA_MAX];
  int saida_n;

  // disco
  disco_unidade_t disco[DISCO_UNIDADES];

  // relógio
  uint16_t rel_contador, rel_limite;

  // controlador de interrupções
  uint8_t mascara; // porta 0030
  uint16_t pendentes; // bit N = interrupção N pendente (N em 8..15)
};

disp_t *disp_cria(mem_t *mem)
{
  disp_t *d = calloc(1, sizeof(disp_t));
  d->mem = mem;
  for (int u = 0; u < DISCO_UNIDADES; u++) {
    d->disco[u].dados = calloc(1, DISCO_TAM_UNIDADE);
  }
  return d;
}

void disp_destroi(disp_t *d)
{
  for (int u = 0; u < DISCO_UNIDADES; u++) {
    if (d->disco[u].caminho != NULL) {
      FILE *f = fopen(d->disco[u].caminho, "wb");
      if (f != NULL) {
        fwrite(d->disco[u].dados, 1, DISCO_TAM_UNIDADE, f);
        fclose(f);
      }
      free(d->disco[u].caminho);
    }
    free(d->disco[u].dados);
  }
  free(d);
}

bool disp_monta_disco(disp_t *d, int unidade, const char *arquivo, char *erro, size_t erro_tam)
{
  if (unidade < 0 || unidade >= DISCO_UNIDADES) {
    snprintf(erro, erro_tam, "unidade de disco invalida: %d", unidade);
    return false;
  }
  disco_unidade_t *un = &d->disco[unidade];

  FILE *f = fopen(arquivo, "rb");
  if (f != NULL) {
    size_t lido = fread(un->dados, 1, DISCO_TAM_UNIDADE, f);
    (void)lido; // se o arquivo for menor, o restante fica zerado (calloc)
    fclose(f);
  }
  un->caminho = strdup(arquivo);
  return true;
}

// ---------------------------------------------------------------- console

static void console_seta_pendente(disp_t *d)
{
  d->pendentes |= (1u << INT_CONSOLE);
}

void console_poe_entrada(disp_t *d, char c)
{
  if (d->entrada_n >= CONSOLE_ENTRADA_MAX) return;
  d->entrada[d->entrada_fim] = c;
  d->entrada_fim = (d->entrada_fim + 1) % CONSOLE_ENTRADA_MAX;
  d->entrada_n++;
  console_seta_pendente(d);
}

static int console_tem_entrada(disp_t *d)
{
  return d->entrada_n > 0;
}

static char console_retira_entrada(disp_t *d)
{
  if (d->entrada_n == 0) return 0;
  char c = d->entrada[d->entrada_inicio];
  d->entrada_inicio = (d->entrada_inicio + 1) % CONSOLE_ENTRADA_MAX;
  d->entrada_n--;
  return c;
}

static void console_escreve_saida(disp_t *d, char c)
{
  if (d->saida_n >= CONSOLE_SAIDA_MAX) {
    memmove(d->saida, d->saida + CONSOLE_SAIDA_MAX / 4, d->saida_n - CONSOLE_SAIDA_MAX / 4);
    d->saida_n -= CONSOLE_SAIDA_MAX / 4;
  }
  d->saida[d->saida_n++] = c;
}

void console_limpa_saida(disp_t *d)
{
  d->saida_n = 0;
}

const char *console_saida(disp_t *d, int *tamanho)
{
  *tamanho = d->saida_n;
  return d->saida;
}

// ------------------------------------------------------------------ disco

static void dispara_operacao_disco(disp_t *d, uint8_t operacao_valor)
{
  uint8_t oo = (operacao_valor >> 6) & 0x03;
  uint8_t uu = operacao_valor & 0x03;

  if (uu >= DISCO_UNIDADES) {
    d->pendentes |= (1u << INT_DISCO);
    return;
  }
  disco_unidade_t *un = &d->disco[uu];

  bool valido = (oo == 0 || oo == 1 || oo == 2) &&
                un->face < DISCO_FACES && un->trilha < DISCO_TRILHAS && un->setor < DISCO_SETORES;

  un->erro = !valido;
  un->ocupado = true;
  un->operacao_pendente = oo;
  un->unidade_pendente = uu;
  un->ticks_restantes = valido ? DISCO_TICKS_OPERACAO : 1;
}

static void completa_operacao_disco(disp_t *d, disco_unidade_t *un)
{
  un->ocupado = false;

  if (!un->erro) {
    uint32_t deslocamento = ((uint32_t)un->face * DISCO_TRILHAS + un->trilha) * DISCO_SETORES + un->setor;
    deslocamento *= DISCO_TAM_SETOR;

    if (un->operacao_pendente == 0) { // leitura: disco -> memória
      for (int i = 0; i < DISCO_TAM_SETOR; i++)
        mem_escreve_byte(d->mem, un->endereco + i, un->dados[deslocamento + i]);
    } else if (un->operacao_pendente == 1) { // escrita: memória -> disco
      for (int i = 0; i < DISCO_TAM_SETOR; i++)
        un->dados[deslocamento + i] = mem_le_byte(d->mem, un->endereco + i);
    } else if (un->operacao_pendente == 2) { // identifica
      mem_escreve_palavra(d->mem, un->endereco + 0, DISCO_FACES);
      mem_escreve_palavra(d->mem, un->endereco + 2, DISCO_TRILHAS);
      mem_escreve_palavra(d->mem, un->endereco + 4, DISCO_SETORES);
      mem_escreve_palavra(d->mem, un->endereco + 6, DISCO_TAM_SETOR);
    }
  }

  d->pendentes |= (1u << INT_DISCO);
}

void disp_estado_disco(disp_t *d, int unidade, bool *ocupado, bool *erro, int *ticks_restantes)
{
  if (unidade < 0 || unidade >= DISCO_UNIDADES) { *ocupado = false; *erro = false; *ticks_restantes = 0; return; }
  disco_unidade_t *un = &d->disco[unidade];
  *ocupado = un->ocupado;
  *erro = un->erro;
  *ticks_restantes = un->ticks_restantes;
}

// ----------------------------------------------------------------- portas

uint8_t disp_le_byte(disp_t *d, uint16_t porta)
{
  switch (porta) {
    case PORTA_CONSOLE_DADOS: {
      char c = console_retira_entrada(d);
      if (!console_tem_entrada(d)) d->pendentes &= ~(1u << INT_CONSOLE);
      return (uint8_t)c;
    }
    case PORTA_CONSOLE_ESTADO: {
      uint8_t estado = 0x01; // bit0: sempre pronta para enviar
      if (console_tem_entrada(d)) estado |= 0x02;
      return estado;
    }
    case PORTA_DISCO_FACE: return d->disco[0].face;
    case PORTA_DISCO_TRILHA: return d->disco[0].trilha;
    case PORTA_DISCO_SETOR: return d->disco[0].setor;
    case PORTA_DISCO_OPERACAO: {
      disco_unidade_t *un = &d->disco[0];
      uint8_t estado = (un->ocupado ? 0x01 : 0) | (un->erro ? 0x02 : 0);
      un->erro = false;
      return estado;
    }
    case PORTA_DISCO_END_ALTO: return (uint8_t)(d->disco[0].endereco >> 8);
    case PORTA_DISCO_END_BAIXO: return (uint8_t)(d->disco[0].endereco & 0xFF);
    case PORTA_RELOGIO_CONT_ALTO: return (uint8_t)(d->rel_contador >> 8);
    case PORTA_RELOGIO_CONT_BAIXO: return (uint8_t)(d->rel_contador & 0xFF);
    case PORTA_RELOGIO_LIM_ALTO: return (uint8_t)(d->rel_limite >> 8);
    case PORTA_RELOGIO_LIM_BAIXO: return (uint8_t)(d->rel_limite & 0xFF);
    case PORTA_CTRL_INTERRUPCOES: return d->mascara;
    default: return 0;
  }
}

void disp_escreve_byte(disp_t *d, uint16_t porta, uint8_t valor)
{
  switch (porta) {
    case PORTA_CONSOLE_DADOS:
      console_escreve_saida(d, (char)valor);
      break;
    case PORTA_CONSOLE_ESTADO:
      break; // somente leitura
    case PORTA_DISCO_FACE: d->disco[0].face = valor; break;
    case PORTA_DISCO_TRILHA: d->disco[0].trilha = valor; break;
    case PORTA_DISCO_SETOR: d->disco[0].setor = valor; break;
    case PORTA_DISCO_OPERACAO: dispara_operacao_disco(d, valor); break;
    case PORTA_DISCO_END_ALTO:
      d->disco[0].endereco = (uint16_t)((d->disco[0].endereco & 0x00FF) | (valor << 8));
      break;
    case PORTA_DISCO_END_BAIXO:
      d->disco[0].endereco = (uint16_t)((d->disco[0].endereco & 0xFF00) | valor);
      break;
    case PORTA_RELOGIO_CONT_ALTO: d->rel_contador = (uint16_t)((d->rel_contador & 0x00FF) | (valor << 8)); break;
    case PORTA_RELOGIO_CONT_BAIXO: d->rel_contador = (uint16_t)((d->rel_contador & 0xFF00) | valor); break;
    case PORTA_RELOGIO_LIM_ALTO: d->rel_limite = (uint16_t)((d->rel_limite & 0x00FF) | (valor << 8)); break;
    case PORTA_RELOGIO_LIM_BAIXO: d->rel_limite = (uint16_t)((d->rel_limite & 0xFF00) | valor); break;
    case PORTA_CTRL_INTERRUPCOES: d->mascara = valor; break;
    default: break;
  }
}

void disp_tick(disp_t *d)
{
  for (int u = 0; u < DISCO_UNIDADES; u++) {
    disco_unidade_t *un = &d->disco[u];
    if (un->ocupado && un->ticks_restantes > 0) {
      un->ticks_restantes--;
      if (un->ticks_restantes == 0) completa_operacao_disco(d, un);
    }
  }

  if (d->rel_limite != 0) {
    d->rel_contador++;
    if (d->rel_contador == d->rel_limite) {
      d->rel_contador = 0;
      d->pendentes |= (1u << INT_RELOGIO);
    }
  }
}

int disp_interrupcao_pendente(disp_t *d)
{
  for (int n = 8; n <= 15; n++) {
    if ((d->pendentes & (1u << n)) && (d->mascara & (1u << (n - 8)))) return n;
  }
  return -1;
}

void disp_confirma_interrupcao(disp_t *d, int numero)
{
  d->pendentes &= ~(1u << numero);
}

uint16_t disp_relogio_contador(disp_t *d) { return d->rel_contador; }
uint16_t disp_relogio_limite(disp_t *d) { return d->rel_limite; }
uint8_t disp_mascara_interrupcoes(disp_t *d) { return d->mascara; }
