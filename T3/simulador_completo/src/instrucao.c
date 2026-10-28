#include "instrucao.h"
#include <string.h>
#include <stdio.h>
#include <strings.h>

// ------------------------------------------------------- nomes de registros

typedef struct { const char *nome; int codigo; } nome_codigo_t;

static const nome_codigo_t REG_NOMES[] = {
  {"r0",0}, {"r1",1}, {"r2",2}, {"r3",3}, {"r4",4},
  {"r5",5}, {"bp",5}, {"r6",6}, {"sp",6}, {"r7",7}, {"ip",7},
};
#define N_REG_NOMES (int)(sizeof(REG_NOMES)/sizeof(REG_NOMES[0]))

static const char *REG_CANONICO[8] = {"r0","r1","r2","r3","r4","bp","sp","ip"};

static const nome_codigo_t SUP_NOMES[] = {
  {"s0",0}, {"sr",0}, {"s1",1}, {"s2",2}, {"s3",3},
  {"s4",4}, {"cs",4}, {"pt",4}, {"s5",5}, {"cl",5},
  {"s6",6}, {"ds",6}, {"s7",7}, {"dl",7},
};
#define N_SUP_NOMES (int)(sizeof(SUP_NOMES)/sizeof(SUP_NOMES[0]))

static const char *SUP_CANONICO[8] = {"sr","s1","s2","s3","cs","cl","ds","dl"};

int reg_nome_para_codigo(const char *nome)
{
  for (int i = 0; i < N_REG_NOMES; i++)
    if (strcasecmp(nome, REG_NOMES[i].nome) == 0) return REG_NOMES[i].codigo;
  return -1;
}

int sup_nome_para_codigo(const char *nome)
{
  for (int i = 0; i < N_SUP_NOMES; i++)
    if (strcasecmp(nome, SUP_NOMES[i].nome) == 0) return SUP_NOMES[i].codigo;
  return -1;
}

const char *reg_codigo_para_nome(int codigo)
{
  if (codigo < 0 || codigo > 7) return "?";
  return REG_CANONICO[codigo];
}

const char *sup_codigo_para_nome(int codigo)
{
  if (codigo < 0 || codigo > 7) return "?";
  return SUP_CANONICO[codigo];
}

// ------------------------------------------------------------- condições

static const nome_codigo_t COND_NOMES[] = {
  {"z",0}, {"eq",0}, {"c",1}, {"lo",1}, {"o",2}, {"n",3}, {"np",3},
  {"nz",4}, {"ne",4}, {"nc",5}, {"hs",5}, {"no",6}, {"p",7},
  {"ge",8}, {"lt",9}, {"gt",10}, {"le",11}, {"hi",12}, {"ls",13},
};
#define N_COND_NOMES (int)(sizeof(COND_NOMES)/sizeof(COND_NOMES[0]))

static const char *COND_CANONICO[16] = {
  "z","c","o","n","nz","nc","no","p","ge","lt","gt","le","hi","ls","?","?"
};

int cond_nome_para_codigo(const char *nome)
{
  for (int i = 0; i < N_COND_NOMES; i++)
    if (strcasecmp(nome, COND_NOMES[i].nome) == 0) return COND_NOMES[i].codigo;
  return -1;
}

const char *cond_codigo_para_nome(int codigo)
{
  if (codigo < 0 || codigo > 15) return "?";
  return COND_CANONICO[codigo];
}

bool cond_verdadeira(int codigo, bool n, bool o, bool z, bool c)
{
  switch (codigo) {
    case 0: return z;
    case 1: return c;
    case 2: return o;
    case 3: return n;
    case 4: return !z;
    case 5: return !c;
    case 6: return !o;
    case 7: return !n;
    case 8: return !(n ^ o);          // GE: N xnor O
    case 9: return (n ^ o);           // LT: N xor O
    case 10: return !z && !(n ^ o);   // GT: NZ and GE
    case 11: return z || (n ^ o);     // LE: Z or LT
    case 12: return !z && !c;         // HI: NZ and NC
    case 13: return z || c;           // LS: Z or C
    default: return false;
  }
}

// -------------------------------------------------- tabela de instruções
//
// A ordem importa: entradas de nomes alternativos (jmp, bra, ret, push,
// pop, pushs, pops, skip, jmpq, braq) aparecem antes das entradas
// genéricas equivalentes (ld, add, st, lds, sts, brac, ldq, addq) com o
// mesmo par formato/codop, porque a decodificação escolhe a primeira
// entrada da tabela cujos campos fixos batem com a palavra lida (tabelas
// 4, 6, 8 e 10 do mancha.pdf).

#define X FORMA_REG_DATA
#define FIXO(a,b) a,b

static const instr_info_t TABELA[] = {
  // formato registrador (00) -- família ld (codop 0000)
  {"ret",  FMT_REGISTRADOR, 0b0000, false, FORMA_RET,          -1, -1},
  {"jmp",  FMT_REGISTRADOR, 0b0000, false, FORMA_DATA_REG7,     7, -1},
  {"pop",  FMT_REGISTRADOR, 0b0000, false, FORMA_REG_MOD_FIXO, FIXO(MOD_POSINC, 6)},
  {"ld",   FMT_REGISTRADOR, 0b0000, false, X,                  -1, -1},
  {"ldb",  FMT_REGISTRADOR, 0b0000, true,  X,                  -1, -1},

  // família st (codop 0001)
  {"push", FMT_REGISTRADOR, 0b0001, false, FORMA_REG_MOD_FIXO, FIXO(MOD_PREDEC, 6)},
  {"st",   FMT_REGISTRADOR, 0b0001, false, X,                  -1, -1},
  {"stb",  FMT_REGISTRADOR, 0b0001, true,  X,                  -1, -1},

  // família add (codop 0010)
  {"bra",  FMT_REGISTRADOR, 0b0010, false, FORMA_DATA_REG7,     7, -1},
  {"add",  FMT_REGISTRADOR, 0b0010, false, X,                  -1, -1},
  {"addb", FMT_REGISTRADOR, 0b0010, true,  X,                  -1, -1},

  {"sub",  FMT_REGISTRADOR, 0b0011, false, X,                  -1, -1},
  {"subb", FMT_REGISTRADOR, 0b0011, true,  X,                  -1, -1},
  {"shl",  FMT_REGISTRADOR, 0b0110, false, X,                  -1, -1},
  {"shlb", FMT_REGISTRADOR, 0b0110, true,  X,                  -1, -1},
  {"shr",  FMT_REGISTRADOR, 0b0111, false, X,                  -1, -1},
  {"shrb", FMT_REGISTRADOR, 0b0111, true,  X,                  -1, -1},
  {"cmp",  FMT_REGISTRADOR, 0b1000, false, X,                  -1, -1},
  {"cmpb", FMT_REGISTRADOR, 0b1000, true,  X,                  -1, -1},
  {"and",  FMT_REGISTRADOR, 0b1001, false, X,                  -1, -1},
  {"andb", FMT_REGISTRADOR, 0b1001, true,  X,                  -1, -1},
  {"or",   FMT_REGISTRADOR, 0b1010, false, X,                  -1, -1},
  {"orb",  FMT_REGISTRADOR, 0b1010, true,  X,                  -1, -1},
  {"xor",  FMT_REGISTRADOR, 0b1011, false, X,                  -1, -1},
  {"xorb", FMT_REGISTRADOR, 0b1011, true,  X,                  -1, -1},
  {"in",   FMT_REGISTRADOR, 0b1100, false, X,                  -1, -1},
  {"inb",  FMT_REGISTRADOR, 0b1100, true,  X,                  -1, -1},
  {"out",  FMT_REGISTRADOR, 0b1101, false, X,                  -1, -1},
  {"outb", FMT_REGISTRADOR, 0b1101, true,  X,                  -1, -1},
  {"swap", FMT_REGISTRADOR, 0b1110, false, X,                  -1, -1},
  {"swapb",FMT_REGISTRADOR, 0b1110, true,  X,                  -1, -1},

  // formato especial (01)
  {"call", FMT_ESPECIAL, 0b00000, false, FORMA_DATA_REG0,       0, -1},
  {"pops", FMT_ESPECIAL, 0b00010, false, FORMA_SUP_MOD_FIXO,  FIXO(MOD_POSINC, 6)},
  {"lds",  FMT_ESPECIAL, 0b00010, false, FORMA_SUP_DATA,       -1, -1},
  {"pushs",FMT_ESPECIAL, 0b00011, false, FORMA_SUP_MOD_FIXO,  FIXO(MOD_PREDEC, 6)},
  {"sts",  FMT_ESPECIAL, 0b00011, false, FORMA_SUP_DATA,       -1, -1},
  {"mul",  FMT_ESPECIAL, 0b01000, false, FORMA_REG_DATA,       -1, -1},
  {"div",  FMT_ESPECIAL, 0b01001, false, FORMA_REG_DATA,       -1, -1},
  {"addc", FMT_ESPECIAL, 0b01010, false, FORMA_REG_DATA,       -1, -1},

  // formato condicional (10)
  {"trap", FMT_CONDICIONAL, 0b0000, false, FORMA_IM6,           0, -1},
  {"skip", FMT_CONDICIONAL, 0b0001, false, FORMA_COND,          4, -1},
  {"brac", FMT_CONDICIONAL, 0b0001, false, FORMA_COND_IM6,     -1, -1},
  {"rete", FMT_CONDICIONAL, 0b0010, false, FORMA_NENHUMA,      -1, -1},
  {"halt", FMT_CONDICIONAL, 0b0011, false, FORMA_NENHUMA,      -1, -1},
  {"di",   FMT_CONDICIONAL, 0b0100, false, FORMA_NENHUMA,      -1, -1},
  {"ei",   FMT_CONDICIONAL, 0b0101, false, FORMA_NENHUMA,      -1, -1},
  {"jmpc", FMT_CONDICIONAL, 0b1000, false, FORMA_COND_DATA,    -1, -1},

  // formato rápido (11)
  {"jmpq", FMT_RAPIDO, 0, false, FORMA_IM10_REG_FIXO,           7, -1},
  {"ldq",  FMT_RAPIDO, 0, false, FORMA_REG_IM10,                -1, -1},
  {"braq", FMT_RAPIDO, 1, false, FORMA_IM10_REG_FIXO,           7, -1},
  {"addq", FMT_RAPIDO, 1, false, FORMA_REG_IM10,                -1, -1},
};
#undef X
#undef FIXO

#define N_TABELA (int)(sizeof(TABELA)/sizeof(TABELA[0]))

const instr_info_t *instrucao_procura(const char *mnemonico)
{
  for (int i = 0; i < N_TABELA; i++)
    if (strcasecmp(mnemonico, TABELA[i].mnemonico) == 0) return &TABELA[i];
  return NULL;
}

const instr_info_t *instrucao_por_indice(int i)
{
  if (i < 0 || i >= N_TABELA) return NULL;
  return &TABELA[i];
}

int instrucao_num_tabela(void) { return N_TABELA; }

// ------------------------------------------------------ empacota/desempacota

static uint16_t estende_sinal(int valor, int bits)
{
  uint16_t mascara = (uint16_t)((1u << bits) - 1);
  return (uint16_t)(valor & mascara);
}

uint16_t instrucao_empacota(int formato, int codop, bool byte_op,
                             int reg, int mod, int dreg,
                             int cond, int im6, int im10)
{
  uint16_t palavra = (uint16_t)((formato & 0x3) << 14);

  switch (formato) {
    case FMT_REGISTRADOR:
      palavra |= (uint16_t)((codop & 0xF) << 10);
      palavra |= (uint16_t)((byte_op ? 1 : 0) << 9);
      palavra |= (uint16_t)((reg & 0x7) << 6);
      palavra |= (uint16_t)((mod & 0x7) << 3);
      palavra |= (uint16_t)(dreg & 0x7);
      break;
    case FMT_ESPECIAL:
      palavra |= (uint16_t)((codop & 0x1F) << 9);
      palavra |= (uint16_t)((reg & 0x7) << 6);
      palavra |= (uint16_t)((mod & 0x7) << 3);
      palavra |= (uint16_t)(dreg & 0x7);
      break;
    case FMT_CONDICIONAL:
      palavra |= (uint16_t)((codop & 0xF) << 10);
      if (mod >= 0) { // jmpc: campo baixo carrega DATA (mod+dreg), não IM6
        palavra |= (uint16_t)((cond & 0xF) << 6);
        palavra |= (uint16_t)((mod & 0x7) << 3);
        palavra |= (uint16_t)(dreg & 0x7);
      } else {
        palavra |= (uint16_t)((cond & 0xF) << 6);
        palavra |= estende_sinal(im6, 6);
      }
      break;
    case FMT_RAPIDO:
      palavra |= (uint16_t)((codop & 0x1) << 13);
      palavra |= (uint16_t)((reg & 0x7) << 10);
      palavra |= estende_sinal(im10, 10);
      break;
  }
  return palavra;
}

static int estende_sinal_leitura(int valor, int bits)
{
  int m = 1 << (bits - 1);
  return (valor ^ m) - m;
}

void instrucao_decodifica(uint16_t palavra, instr_decod_t *out)
{
  memset(out, 0, sizeof(*out));
  out->formato = (palavra >> 14) & 0x3;
  out->reg = -1; out->mod = -1; out->dreg = -1; out->cond = -1;
  out->im6 = 0; out->im10 = 0;
  out->info = NULL;

  int reg = -1, mod = -1, dreg = -1, cond = -1, im6 = 0, im10 = 0, codop = 0;
  bool byte_op = false;

  switch (out->formato) {
    case FMT_REGISTRADOR:
      codop = (palavra >> 10) & 0xF;
      byte_op = (palavra >> 9) & 0x1;
      reg = (palavra >> 6) & 0x7;
      mod = (palavra >> 3) & 0x7;
      dreg = palavra & 0x7;
      break;
    case FMT_ESPECIAL:
      codop = (palavra >> 9) & 0x1F;
      reg = (palavra >> 6) & 0x7;
      mod = (palavra >> 3) & 0x7;
      dreg = palavra & 0x7;
      break;
    case FMT_CONDICIONAL:
      codop = (palavra >> 10) & 0xF;
      cond = (palavra >> 6) & 0xF;
      mod = (palavra >> 3) & 0x7;   // válido se a instrução for jmpc
      dreg = palavra & 0x7;         // idem
      im6 = estende_sinal_leitura(palavra & 0x3F, 6);
      break;
    case FMT_RAPIDO:
      codop = (palavra >> 13) & 0x1;
      reg = (palavra >> 10) & 0x7;
      im10 = estende_sinal_leitura(palavra & 0x3FF, 10);
      break;
  }

  out->codop = codop;
  out->byte_op = byte_op;
  out->reg = reg;
  out->mod = mod;
  out->dreg = dreg;
  out->cond = cond;
  out->im6 = im6;
  out->im10 = im10;

  for (int i = 0; i < N_TABELA; i++) {
    const instr_info_t *e = &TABELA[i];
    if (e->formato != out->formato || e->codop != codop) continue;
    if (e->formato == FMT_REGISTRADOR && e->byte_op != byte_op) continue;

    bool bate = false;
    switch (e->forma) {
      case FORMA_REG_DATA:
      case FORMA_SUP_DATA:
      case FORMA_REG_IM10:
      case FORMA_DATA_REG0:
      case FORMA_COND_IM6:
      case FORMA_IM6:
      case FORMA_NENHUMA:
      case FORMA_COND_DATA:
        bate = true;
        break;
      case FORMA_DATA_REG7:
      case FORMA_IM10_REG_FIXO:
        bate = (reg == e->fixo);
        break;
      case FORMA_COND:
        bate = (im6 == e->fixo);
        break;
      case FORMA_RET:
        bate = (reg == 7 && mod == MOD_POSINC && dreg == 6);
        break;
      case FORMA_REG_MOD_FIXO:
      case FORMA_SUP_MOD_FIXO:
        bate = (mod == e->fixo && dreg == e->fixo2);
        break;
    }
    if (bate) { out->info = e; break; }
  }

  if (out->formato == FMT_CONDICIONAL && out->info != NULL && out->info->forma != FORMA_COND_DATA) {
    // não é jmpc: os campos mod/dreg lidos acima não fazem sentido aqui
    out->mod = -1; out->dreg = -1;
  }
}

bool instrucao_requer_imediato(const instr_decod_t *d)
{
  if (d->mod < 0) return false;
  return d->mod == MOD_IMED || d->mod == MOD_ABS || d->mod == MOD_DESLOC;
}

static void formata_data(char *saida, size_t tam, int mod, int dreg, uint16_t imediato)
{
  switch (mod) {
    case MOD_REG: snprintf(saida, tam, "%s", reg_codigo_para_nome(dreg)); break;
    case MOD_IND: snprintf(saida, tam, "(%s)", reg_codigo_para_nome(dreg)); break;
    case MOD_POSINC: snprintf(saida, tam, "(%s+)", reg_codigo_para_nome(dreg)); break;
    case MOD_PREDEC: snprintf(saida, tam, "(-%s)", reg_codigo_para_nome(dreg)); break;
    case MOD_IMED: snprintf(saida, tam, "%d", (int16_t)imediato); break;
    case MOD_ABS: snprintf(saida, tam, "(0x%04X)", imediato); break;
    case MOD_DESLOC: snprintf(saida, tam, "(%s+%d)", reg_codigo_para_nome(dreg), (int16_t)imediato); break;
    default: snprintf(saida, tam, "?"); break;
  }
}

void instrucao_desmonta(const instr_decod_t *d, uint16_t imediato, char *saida, size_t tam)
{
  if (d->info == NULL) { snprintf(saida, tam, "??? (0x%04X)", 0); return; }

  char data[24];
  const instr_info_t *e = d->info;

  switch (e->forma) {
    case FORMA_REG_DATA:
      formata_data(data, sizeof(data), d->mod, d->dreg, imediato);
      snprintf(saida, tam, "%s %s, %s", e->mnemonico, reg_codigo_para_nome(d->reg), data);
      break;
    case FORMA_SUP_DATA:
      formata_data(data, sizeof(data), d->mod, d->dreg, imediato);
      snprintf(saida, tam, "%s %s, %s", e->mnemonico, sup_codigo_para_nome(d->reg), data);
      break;
    case FORMA_REG_IM10:
      snprintf(saida, tam, "%s %s, %d", e->mnemonico, reg_codigo_para_nome(d->reg), d->im10);
      break;
    case FORMA_DATA_REG7:
    case FORMA_DATA_REG0:
      formata_data(data, sizeof(data), d->mod, d->dreg, imediato);
      snprintf(saida, tam, "%s %s", e->mnemonico, data);
      break;
    case FORMA_COND_IM6:
      snprintf(saida, tam, "brac %s, %d", cond_codigo_para_nome(d->cond), d->im6);
      break;
    case FORMA_COND:
      snprintf(saida, tam, "skip %s", cond_codigo_para_nome(d->cond));
      break;
    case FORMA_IM6:
      snprintf(saida, tam, "trap %d", d->im6 & 0x3F);
      break;
    case FORMA_NENHUMA:
    case FORMA_RET:
      snprintf(saida, tam, "%s", e->mnemonico);
      break;
    case FORMA_REG_MOD_FIXO:
      snprintf(saida, tam, "%s %s", e->mnemonico, reg_codigo_para_nome(d->reg));
      break;
    case FORMA_SUP_MOD_FIXO:
      snprintf(saida, tam, "%s %s", e->mnemonico, sup_codigo_para_nome(d->reg));
      break;
    case FORMA_IM10_REG_FIXO:
      snprintf(saida, tam, "%s %d", e->mnemonico, d->im10);
      break;
    case FORMA_COND_DATA:
      formata_data(data, sizeof(data), d->mod, d->dreg, imediato);
      snprintf(saida, tam, "jmpc %s, %s", cond_codigo_para_nome(d->cond), data);
      break;
  }
}
