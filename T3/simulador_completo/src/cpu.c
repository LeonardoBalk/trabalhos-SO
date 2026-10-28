#include "cpu.h"
#include "instrucao.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct cpu {
  uint16_t r[8]; // r0-r4, bp, sp, ip
  uint16_t s[8]; // sr, s1-s3, cs, cl, ds, dl (ou pt no lugar de cs, com paginação)
  bool parada;
  long n_instrucoes;
  mem_t *mem;
  disp_t *disp;
  char ultimo_evento[64];
};

cpu_t *cpu_cria(mem_t *mem, disp_t *disp)
{
  cpu_t *cpu = calloc(1, sizeof(cpu_t));
  cpu->mem = mem;
  cpu->disp = disp;
  return cpu;
}

void cpu_destroi(cpu_t *cpu) { free(cpu); }

bool cpu_bit(cpu_t *cpu, uint16_t mascara) { return (cpu->s[0] & mascara) != 0; }
bool cpu_modo_supervisor(cpu_t *cpu) { return cpu_bit(cpu, SR_S); }
bool cpu_paginacao_ativa(cpu_t *cpu) { return cpu_bit(cpu, SR_P); }
uint16_t cpu_r(cpu_t *cpu, int i) { return cpu->r[i & 7]; }
uint16_t cpu_s(cpu_t *cpu, int i) { return cpu->s[i & 7]; }
uint16_t cpu_sr(cpu_t *cpu) { return cpu->s[0]; }
bool cpu_parada(cpu_t *cpu) { return cpu->parada; }
long cpu_num_instrucoes(cpu_t *cpu) { return cpu->n_instrucoes; }
const char *cpu_ultimo_evento(cpu_t *cpu) { return cpu->ultimo_evento; }

// -------------------------------------------------------------- tradução

typedef struct { uint32_t endereco; bool ok; int causa; } trad_t;

static trad_t traduz(cpu_t *cpu, uint16_t virt, bool dados, bool escrita)
{
  trad_t r = { .endereco = virt, .ok = true, .causa = 0 };
  bool supervisor = cpu_bit(cpu, SR_S);

  if (cpu_bit(cpu, SR_P)) {
    int pagina = (virt >> 9) & 0x7F;
    if (dados) pagina += 128;
    uint32_t end_entrada = (uint32_t)cpu->s[4] + (uint32_t)pagina * 2;
    uint16_t entrada = mem_le_palavra(cpu->mem, end_entrada);
    int controle = (entrada >> 13) & 0x7;
    int tipo = controle >> 1;
    int bit_acesso = controle & 1;
    uint32_t quadro = entrada & 0x1FFF;

    if (tipo == 0) { r.ok = false; r.causa = 5; return r; } // ausência de quadro
    if (!supervisor && escrita && tipo == 1) { r.ok = false; r.causa = 1; return r; } // só leitura

    int novo_controle = controle;
    if (escrita && tipo == 2) novo_controle = (3 << 1) | 1; // promove a modificado
    else if (bit_acesso == 0) novo_controle = (tipo << 1) | 1;
    if (novo_controle != controle) {
      uint16_t nova = (uint16_t)((novo_controle << 13) | quadro);
      mem_escreve_palavra(cpu->mem, end_entrada, nova);
    }
    r.endereco = quadro * 512 + (virt & 0x1FF);
    return r;
  }

  uint16_t base = dados ? cpu->s[6] : cpu->s[4];   // ds ou cs
  uint16_t limite = dados ? cpu->s[7] : cpu->s[5];  // dl ou cl
  if (!supervisor && limite != 0 && virt >= limite) { r.ok = false; r.causa = 1; return r; }
  r.endereco = (uint32_t)base + virt;
  return r;
}

uint32_t cpu_traduz_exibicao(cpu_t *cpu, uint16_t virt, bool dados)
{
  if (cpu_bit(cpu, SR_P)) {
    int pagina = (virt >> 9) & 0x7F;
    if (dados) pagina += 128;
    uint32_t end_entrada = (uint32_t)cpu->s[4] + (uint32_t)pagina * 2;
    uint16_t entrada = mem_le_palavra(cpu->mem, end_entrada);
    int tipo = ((entrada >> 13) & 0x7) >> 1;
    uint32_t quadro = entrada & 0x1FFF;
    if (tipo == 0) return virt;
    return quadro * 512 + (virt & 0x1FF);
  }
  uint16_t base = dados ? cpu->s[6] : cpu->s[4];
  return (uint32_t)base + virt;
}

// forward
static void cpu_interrompe(cpu_t *cpu, int numero);

static bool le_instrucao_palavra(cpu_t *cpu, uint16_t *valor)
{
  trad_t t = traduz(cpu, cpu->r[7], false, false);
  if (!t.ok) { cpu_interrompe(cpu, t.causa); return false; }
  *valor = mem_le_palavra(cpu->mem, t.endereco);
  cpu->r[7] = (uint16_t)(cpu->r[7] + 2);
  return true;
}

static bool le_dado(cpu_t *cpu, uint16_t virt, bool byte_op, uint16_t *valor)
{
  trad_t t = traduz(cpu, virt, true, false);
  if (!t.ok) { cpu_interrompe(cpu, t.causa); return false; }
  *valor = byte_op ? mem_le_byte(cpu->mem, t.endereco) : mem_le_palavra(cpu->mem, t.endereco);
  return true;
}

static bool escreve_dado(cpu_t *cpu, uint16_t virt, bool byte_op, uint16_t valor)
{
  trad_t t = traduz(cpu, virt, true, true);
  if (!t.ok) { cpu_interrompe(cpu, t.causa); return false; }
  if (byte_op) mem_escreve_byte(cpu->mem, t.endereco, (uint8_t)valor);
  else mem_escreve_palavra(cpu->mem, t.endereco, valor);
  return true;
}

// --------------------------------------------------------- modos DATA

static bool busca_operando(cpu_t *cpu, int mod, int dreg, uint16_t imediato, bool byte_op, uint16_t *valor)
{
  switch (mod) {
    case MOD_REG: {
      uint16_t v = cpu->r[dreg];
      *valor = byte_op ? (uint16_t)(v & 0xFF) : v;
      return true;
    }
    case MOD_IND:
      return le_dado(cpu, cpu->r[dreg], byte_op, valor);
    case MOD_POSINC: {
      uint16_t end = cpu->r[dreg];
      if (!le_dado(cpu, end, byte_op, valor)) return false;
      cpu->r[dreg] = (uint16_t)(end + (byte_op ? 1 : 2));
      return true;
    }
    case MOD_PREDEC:
      cpu->r[dreg] = (uint16_t)(cpu->r[dreg] - (byte_op ? 1 : 2));
      return le_dado(cpu, cpu->r[dreg], byte_op, valor);
    case MOD_IMED:
      *valor = byte_op ? (uint16_t)(imediato & 0xFF) : imediato;
      return true;
    case MOD_ABS:
      return le_dado(cpu, imediato, byte_op, valor);
    case MOD_DESLOC:
      return le_dado(cpu, (uint16_t)(cpu->r[dreg] + imediato), byte_op, valor);
    default:
      *valor = 0;
      return true;
  }
}

static bool grava_operando(cpu_t *cpu, int mod, int dreg, uint16_t imediato, bool byte_op, uint16_t valor)
{
  switch (mod) {
    case MOD_REG:
      if (byte_op) cpu->r[dreg] = (uint16_t)((cpu->r[dreg] & 0xFF00) | (valor & 0xFF));
      else cpu->r[dreg] = valor;
      return true;
    case MOD_IND:
      return escreve_dado(cpu, cpu->r[dreg], byte_op, valor);
    case MOD_POSINC: {
      uint16_t end = cpu->r[dreg];
      if (!escreve_dado(cpu, end, byte_op, valor)) return false;
      cpu->r[dreg] = (uint16_t)(end + (byte_op ? 1 : 2));
      return true;
    }
    case MOD_PREDEC:
      cpu->r[dreg] = (uint16_t)(cpu->r[dreg] - (byte_op ? 1 : 2));
      return escreve_dado(cpu, cpu->r[dreg], byte_op, valor);
    case MOD_IMED:
      return true; // inválido como destino; montador nunca gera isto
    case MOD_ABS:
      return escreve_dado(cpu, imediato, byte_op, valor);
    case MOD_DESLOC:
      return escreve_dado(cpu, (uint16_t)(cpu->r[dreg] + imediato), byte_op, valor);
    default:
      return true;
  }
}

// os dispositivos simulados (mancha.pdf, secção 13, tabela 17) são todos
// registrados de 8 bits em portas individuais -- inclusive o relógio, que
// expõe seus valores de 16 bits como dois registradores de 8 bits em
// portas adjacentes (não como uma porta de 16 bits). Por isso "in"/"out"
// (largura palavra) acessam a mesma porta de 8 bits que "inb"/"outb",
// só que estendendo o sinal/zero para 16 bits na leitura; eles nunca
// tocam a porta seguinte.
static uint16_t porta_le(disp_t *disp, uint16_t porta, bool byte_op)
{
  (void)byte_op;
  return disp_le_byte(disp, porta);
}

static void porta_escreve(disp_t *disp, uint16_t porta, bool byte_op, uint16_t valor)
{
  (void)byte_op;
  disp_escreve_byte(disp, porta, (uint8_t)valor);
}

static bool busca_operando_porta(cpu_t *cpu, int mod, int dreg, uint16_t imediato, bool byte_op, uint16_t *valor)
{
  uint16_t porta;
  switch (mod) {
    case MOD_IND: porta = cpu->r[dreg]; break;
    case MOD_POSINC: porta = cpu->r[dreg]; cpu->r[dreg] = (uint16_t)(cpu->r[dreg] + (byte_op ? 1 : 2)); break;
    case MOD_PREDEC: cpu->r[dreg] = (uint16_t)(cpu->r[dreg] - (byte_op ? 1 : 2)); porta = cpu->r[dreg]; break;
    case MOD_ABS: porta = imediato; break;
    case MOD_DESLOC: porta = (uint16_t)(cpu->r[dreg] + imediato); break;
    default: *valor = 0; return true;
  }
  *valor = porta_le(cpu->disp, porta, byte_op);
  return true;
}

static void grava_operando_porta(cpu_t *cpu, int mod, int dreg, uint16_t imediato, bool byte_op, uint16_t valor)
{
  uint16_t porta;
  switch (mod) {
    case MOD_IND: porta = cpu->r[dreg]; break;
    case MOD_POSINC: porta = cpu->r[dreg]; cpu->r[dreg] = (uint16_t)(cpu->r[dreg] + (byte_op ? 1 : 2)); break;
    case MOD_PREDEC: cpu->r[dreg] = (uint16_t)(cpu->r[dreg] - (byte_op ? 1 : 2)); porta = cpu->r[dreg]; break;
    case MOD_ABS: porta = imediato; break;
    case MOD_DESLOC: porta = (uint16_t)(cpu->r[dreg] + imediato); break;
    default: return;
  }
  porta_escreve(cpu->disp, porta, byte_op, valor);
}

// ---------------------------------------------------------- pilha interna

static bool empilha_dado(cpu_t *cpu, uint16_t valor)
{
  cpu->r[6] = (uint16_t)(cpu->r[6] - 2);
  trad_t t = traduz(cpu, cpu->r[6], true, true);
  if (!t.ok) { cpu_interrompe(cpu, t.causa); return false; }
  mem_escreve_palavra(cpu->mem, t.endereco, valor);
  return true;
}

static uint16_t desempilha_dado(cpu_t *cpu, bool *ok)
{
  trad_t t = traduz(cpu, cpu->r[6], true, false);
  if (!t.ok) { cpu_interrompe(cpu, t.causa); *ok = false; return 0; }
  uint16_t v = mem_le_palavra(cpu->mem, t.endereco);
  cpu->r[6] = (uint16_t)(cpu->r[6] + 2);
  *ok = true;
  return v;
}

// ---------------------------------------------------------------- flags

static void seta_flag(cpu_t *cpu, uint16_t mascara, bool valor)
{
  if (valor) cpu->s[0] |= mascara; else cpu->s[0] &= (uint16_t)~mascara;
}

static void marca_nz(cpu_t *cpu, uint16_t resultado)
{
  seta_flag(cpu, SR_N, (resultado & 0x8000) != 0);
  seta_flag(cpu, SR_Z, resultado == 0);
}

static uint16_t soma_com_flags(cpu_t *cpu, uint16_t a, uint16_t b, int carry_extra)
{
  int32_t resultado = (int32_t)a + (int32_t)b + carry_extra;
  uint16_t r16 = (uint16_t)resultado;
  bool overflow = ((~(a ^ b)) & (a ^ r16) & 0x8000) != 0;
  seta_flag(cpu, SR_C, resultado > 0xFFFF);
  seta_flag(cpu, SR_O, overflow);
  marca_nz(cpu, r16);
  return r16;
}

static uint16_t subtrai_com_flags(cpu_t *cpu, uint16_t a, uint16_t b)
{
  int32_t resultado = (int32_t)a - (int32_t)b;
  uint16_t r16 = (uint16_t)resultado;
  bool overflow = ((a ^ b) & (a ^ r16) & 0x8000) != 0;
  seta_flag(cpu, SR_C, a < b); // C=1: houve borrow, REG < DATA sem sinal (tabela 2, LO)
  seta_flag(cpu, SR_O, overflow);
  marca_nz(cpu, r16);
  return r16;
}

static uint16_t logica_com_flags(cpu_t *cpu, uint16_t resultado)
{
  seta_flag(cpu, SR_O, false);
  seta_flag(cpu, SR_C, false);
  marca_nz(cpu, resultado);
  return resultado;
}

static uint16_t desloca(cpu_t *cpu, uint16_t valor, uint16_t contagem, bool esquerda)
{
  bool ultimo = cpu_bit(cpu, SR_Z), perdeu = false;
  for (int i = 0; i < contagem && i < 16; i++) {
    bool saiu = esquerda ? ((valor & 0x8000) != 0) : ((valor & 1) != 0);
    if (saiu) perdeu = true;
    ultimo = saiu;
    valor = esquerda ? (uint16_t)(valor << 1) : (uint16_t)(valor >> 1);
  }
  seta_flag(cpu, SR_N, (valor & 0x8000) != 0);
  seta_flag(cpu, SR_Z, ultimo);
  seta_flag(cpu, SR_O, perdeu);
  return valor;
}

// ----------------------------------------------------------- interrupções

static const char *nome_interrupcao(int n)
{
  switch (n) {
    case 0: return "inicio de operacao";
    case 1: return "violacao de segmento";
    case 2: return "instrucao privilegiada";
    case 3: return "divisao por zero";
    case 4: return "instrucao ilegal";
    case 5: return "ausencia de quadro";
    case 8: return "console";
    case 9: return "disco";
    case 10: return "relogio";
    default: return "interrupcao externa/trap";
  }
}

static void cpu_interrompe(cpu_t *cpu, int numero)
{
  numero &= 0xF;
  uint32_t end_frame = (uint32_t)numero * 8;
  uint16_t novo_ip = mem_le_palavra(cpu->mem, end_frame + 0);
  uint16_t novo_sp = mem_le_palavra(cpu->mem, end_frame + 2);
  uint16_t novo_cs = mem_le_palavra(cpu->mem, end_frame + 4);
  uint16_t novo_ds = mem_le_palavra(cpu->mem, end_frame + 6);
  if (novo_ip == 0) return; // quadro não definido: interrupção ignorada

  uint16_t r_antigo[8], s_antigo[8];
  memcpy(r_antigo, cpu->r, sizeof(r_antigo));
  memcpy(s_antigo, cpu->s, sizeof(s_antigo));

  cpu->r[6] = novo_sp;
  cpu->r[7] = novo_ip;
  cpu->s[4] = novo_cs;
  cpu->s[6] = novo_ds;
  cpu->s[0] |= (SR_S | SR_I | SR_D);

  uint16_t ordem[16] = {
    s_antigo[7], s_antigo[6], s_antigo[5], s_antigo[4],
    s_antigo[3], s_antigo[2], s_antigo[1], s_antigo[0],
    r_antigo[7], r_antigo[6], r_antigo[5], r_antigo[4],
    r_antigo[3], r_antigo[2], r_antigo[1], r_antigo[0],
  };
  for (int i = 0; i < 16; i++)
    if (!empilha_dado(cpu, ordem[i])) break;

  snprintf(cpu->ultimo_evento, sizeof(cpu->ultimo_evento), "%s", nome_interrupcao(numero));
}

static void retorna_excecao(cpu_t *cpu)
{
  uint16_t val[16];
  bool ok;
  for (int i = 0; i < 16; i++) {
    val[i] = desempilha_dado(cpu, &ok);
    if (!ok) return;
  }
  cpu->r[0] = val[0]; cpu->r[1] = val[1]; cpu->r[2] = val[2]; cpu->r[3] = val[3];
  cpu->r[4] = val[4]; cpu->r[5] = val[5];
  cpu->r[6] = val[6]; // sp restaurado
  cpu->r[7] = val[7]; // ip restaurado
  for (int i = 0; i < 8; i++) cpu->s[i] = val[8 + i];
}

// -------------------------------------------------------------- execução

static void executa(cpu_t *cpu, const instr_decod_t *d, uint16_t imediato)
{
  bool supervisor = cpu_bit(cpu, SR_S);
  uint16_t v, resultado;

  switch (d->formato) {
    case FMT_REGISTRADOR:
      switch (d->codop) {
        case 0x0: // ld
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          if (d->byte_op) cpu->r[d->reg] = (uint16_t)((cpu->r[d->reg] & 0xFF00) | (v & 0xFF));
          else cpu->r[d->reg] = v;
          marca_nz(cpu, cpu->r[d->reg]);
          break;
        case 0x1: // st
          grava_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, cpu->r[d->reg]);
          break;
        case 0x2: // add
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          resultado = soma_com_flags(cpu, cpu->r[d->reg], v, 0);
          cpu->r[d->reg] = d->byte_op ? (uint16_t)((cpu->r[d->reg] & 0xFF00) | (resultado & 0xFF)) : resultado;
          break;
        case 0x3: // sub
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          resultado = subtrai_com_flags(cpu, cpu->r[d->reg], v);
          cpu->r[d->reg] = d->byte_op ? (uint16_t)((cpu->r[d->reg] & 0xFF00) | (resultado & 0xFF)) : resultado;
          break;
        case 0x6: // shl
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          cpu->r[d->reg] = desloca(cpu, cpu->r[d->reg], v, true);
          break;
        case 0x7: // shr
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          cpu->r[d->reg] = desloca(cpu, cpu->r[d->reg], v, false);
          break;
        case 0x8: // cmp
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          subtrai_com_flags(cpu, cpu->r[d->reg], v);
          break;
        case 0x9: // and
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          cpu->r[d->reg] = logica_com_flags(cpu, (uint16_t)(cpu->r[d->reg] & v));
          break;
        case 0xA: // or
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          cpu->r[d->reg] = logica_com_flags(cpu, (uint16_t)(cpu->r[d->reg] | v));
          break;
        case 0xB: // xor
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          cpu->r[d->reg] = logica_com_flags(cpu, (uint16_t)(cpu->r[d->reg] ^ v));
          break;
        case 0xC: // in
          if (!cpu_bit(cpu, SR_I)) { cpu_interrompe(cpu, 2); return; }
          if (!busca_operando_porta(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          if (d->byte_op) cpu->r[d->reg] = (uint16_t)((cpu->r[d->reg] & 0xFF00) | (v & 0xFF));
          else cpu->r[d->reg] = v;
          marca_nz(cpu, cpu->r[d->reg]);
          break;
        case 0xD: // out
          if (!cpu_bit(cpu, SR_I)) { cpu_interrompe(cpu, 2); return; }
          grava_operando_porta(cpu, d->mod, d->dreg, imediato, d->byte_op, cpu->r[d->reg]);
          break;
        case 0xE: // swap
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, &v)) return;
          grava_operando(cpu, d->mod, d->dreg, imediato, d->byte_op, cpu->r[d->reg]);
          if (d->byte_op) cpu->r[d->reg] = (uint16_t)((cpu->r[d->reg] & 0xFF00) | (v & 0xFF));
          else cpu->r[d->reg] = v;
          marca_nz(cpu, cpu->r[d->reg]);
          break;
        default:
          cpu_interrompe(cpu, 4);
      }
      break;

    case FMT_ESPECIAL:
      switch (d->codop) {
        case 0x00: { // call
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, false, &v)) return;
          uint16_t retorno = cpu->r[7];
          if (!empilha_dado(cpu, retorno)) return;
          cpu->r[7] = v;
          break;
        }
        case 0x02: // lds
          if (!supervisor) { cpu_interrompe(cpu, 2); return; }
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, false, &v)) return;
          cpu->s[d->reg] = v;
          marca_nz(cpu, cpu->s[d->reg]);
          break;
        case 0x03: // sts
          grava_operando(cpu, d->mod, d->dreg, imediato, false, cpu->s[d->reg]);
          break;
        case 0x08: { // mul
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, false, &v)) return;
          int32_t p = (int32_t)(int16_t)cpu->r[d->reg] * (int32_t)(int16_t)v;
          uint16_t r16 = (uint16_t)p;
          bool overflow = (p != (int32_t)(int16_t)r16);
          seta_flag(cpu, SR_O, overflow);
          seta_flag(cpu, SR_C, overflow);
          marca_nz(cpu, r16);
          cpu->r[d->reg] = r16;
          break;
        }
        case 0x09: { // div
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, false, &v)) return;
          if (v == 0) { cpu_interrompe(cpu, 3); return; }
          int16_t a = (int16_t)cpu->r[d->reg], b = (int16_t)v;
          bool overflow = (a == -32768 && b == -1);
          int32_t q = overflow ? -32768 : (a / b);
          uint16_t r16 = (uint16_t)q;
          seta_flag(cpu, SR_O, overflow);
          seta_flag(cpu, SR_C, false);
          marca_nz(cpu, r16);
          cpu->r[d->reg] = r16;
          break;
        }
        case 0x0A: // addc
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, false, &v)) return;
          resultado = soma_com_flags(cpu, cpu->r[d->reg], v, cpu_bit(cpu, SR_C) ? 1 : 0);
          cpu->r[d->reg] = resultado;
          break;
        default:
          cpu_interrompe(cpu, 4);
      }
      break;

    case FMT_CONDICIONAL:
      switch (d->codop) {
        case 0x0: // trap
          cpu_interrompe(cpu, d->im6 & 0xF);
          break;
        case 0x1: // brac / skip
          if (cond_verdadeira(d->cond, cpu_bit(cpu, SR_N), cpu_bit(cpu, SR_O), cpu_bit(cpu, SR_Z), cpu_bit(cpu, SR_C)))
            cpu->r[7] = (uint16_t)(cpu->r[7] + d->im6);
          break;
        case 0x2: // rete
          if (!supervisor) { cpu_interrompe(cpu, 2); return; }
          retorna_excecao(cpu);
          break;
        case 0x3: // halt
          if (!supervisor) { cpu_interrompe(cpu, 2); return; }
          cpu->parada = true;
          break;
        case 0x4: // di
          if (!supervisor) { cpu_interrompe(cpu, 2); return; }
          seta_flag(cpu, SR_D, true);
          break;
        case 0x5: // ei
          if (!supervisor) { cpu_interrompe(cpu, 2); return; }
          seta_flag(cpu, SR_D, false);
          break;
        case 0x8: // jmpc
          if (!busca_operando(cpu, d->mod, d->dreg, imediato, false, &v)) return;
          if (cond_verdadeira(d->cond, cpu_bit(cpu, SR_N), cpu_bit(cpu, SR_O), cpu_bit(cpu, SR_Z), cpu_bit(cpu, SR_C)))
            cpu->r[7] = v;
          break;
        default:
          cpu_interrompe(cpu, 4);
      }
      break;

    case FMT_RAPIDO:
      if (d->codop == 0) { // ldq
        cpu->r[d->reg] = (uint16_t)d->im10;
        marca_nz(cpu, cpu->r[d->reg]);
      } else { // addq
        resultado = soma_com_flags(cpu, cpu->r[d->reg], (uint16_t)d->im10, 0);
        cpu->r[d->reg] = resultado;
      }
      break;
  }
}

// --------------------------------------------------------------- público

void cpu_liga(cpu_t *cpu)
{
  cpu_interrompe(cpu, 0);
}

void cpu_reinicia(cpu_t *cpu)
{
  memset(cpu->r, 0, sizeof(cpu->r));
  memset(cpu->s, 0, sizeof(cpu->s));
  cpu->parada = false;
  cpu->n_instrucoes = 0;
  cpu->ultimo_evento[0] = '\0';
  cpu_liga(cpu);
}

void cpu_executa_1(cpu_t *cpu)
{
  if (cpu->parada) return;

  if (!cpu_bit(cpu, SR_D)) {
    int numero = disp_interrupcao_pendente(cpu->disp);
    if (numero >= 0) {
      disp_confirma_interrupcao(cpu->disp, numero);
      cpu_interrompe(cpu, numero);
      cpu->n_instrucoes++;
      disp_tick(cpu->disp);
      return;
    }
  }

  uint16_t palavra;
  if (!le_instrucao_palavra(cpu, &palavra)) goto fim;

  instr_decod_t d;
  instrucao_decodifica(palavra, &d);

  if (d.info == NULL) { cpu_interrompe(cpu, 4); goto fim; }

  uint16_t imediato = 0;
  if (instrucao_requer_imediato(&d)) {
    if (!le_instrucao_palavra(cpu, &imediato)) goto fim;
  }

  executa(cpu, &d, imediato);

fim:
  cpu->n_instrucoes++;
  disp_tick(cpu->disp);
}

void cpu_desmonta_proxima(cpu_t *cpu, char *saida, size_t tam)
{
  uint32_t end = cpu_traduz_exibicao(cpu, cpu->r[7], false);
  uint16_t palavra = mem_le_palavra(cpu->mem, end);

  instr_decod_t d;
  instrucao_decodifica(palavra, &d);

  uint16_t imediato = 0;
  if (instrucao_requer_imediato(&d)) {
    uint32_t end2 = cpu_traduz_exibicao(cpu, (uint16_t)(cpu->r[7] + 2), false);
    imediato = mem_le_palavra(cpu->mem, end2);
  }
  instrucao_desmonta(&d, imediato, saida, tam);
}
