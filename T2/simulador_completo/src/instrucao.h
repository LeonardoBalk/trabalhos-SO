// instrucao.h -- codificação/decodificação das instruções do Mancha
// completo (mancha.pdf, secções 5, 6 e 9), usada tanto pelo montador
// quanto pelo simulador (fonte única da verdade sobre o formato binário).
//
// Os quatro formatos de instrução (registrador=0, especial=1,
// condicional=2, rápido=3) e a tabela de códigos de operação foram
// conferidos byte a byte contra o exemplo montado da figura 13/14 do PDF
// (instruções "push bp" = 055E e "ld bp,sp" = 0146).

#ifndef INSTRUCAO_H
#define INSTRUCAO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FMT_REGISTRADOR 0
#define FMT_ESPECIAL    1
#define FMT_CONDICIONAL 2
#define FMT_RAPIDO      3

// modos de endereçamento (tabela 15)
#define MOD_REG     0
#define MOD_IND     1
#define MOD_POSINC  2
#define MOD_PREDEC  3
#define MOD_IMED    4
#define MOD_ABS     5
#define MOD_DESLOC  6

// formas de operando reconhecidas pelo montador/desmontador
typedef enum {
  FORMA_REG_DATA,   // ld, add, cmp, in, out, swap, ...
  FORMA_SUP_DATA,   // lds, sts
  FORMA_REG_IM10,   // ldq, addq
  FORMA_DATA_REG7,  // jmp, bra (REG fixo = ip)
  FORMA_DATA_REG0,  // call (REG fixo = 0, não usado)
  FORMA_COND_IM6,   // brac
  FORMA_COND,       // skip (IM6 fixo = 4)
  FORMA_IM6,        // trap
  FORMA_NENHUMA,    // rete, halt, di, ei (formato condicional, sem operandos)
  FORMA_RET,        // ret (formato registrador, todos os campos fixos)
  FORMA_REG_MOD_FIXO,   // push, pop (MOD/REG-endereço fixos)
  FORMA_SUP_MOD_FIXO,   // pushs, pops
  FORMA_IM10_REG_FIXO,  // jmpq, braq (REG fixo = ip)
  FORMA_COND_DATA,      // jmpc
} forma_operandos_t;

typedef struct {
  const char *mnemonico;
  int formato;
  int codop;
  bool byte_op;       // usa o bit "tam" (somente formato registrador)
  forma_operandos_t forma;
  int fixo;            // valor fixo (REG/MOD/DREG conforme a forma), -1 se N/A
  int fixo2;
} instr_info_t;

// tabela de instruções reconhecidas (montador e desmontador)
const instr_info_t *instrucao_procura(const char *mnemonico);
const instr_info_t *instrucao_por_indice(int i);
int instrucao_num_tabela(void);

// nomes de registradores/condições
int reg_nome_para_codigo(const char *nome);           // -1 se inválido
int sup_nome_para_codigo(const char *nome);            // -1 se inválido
const char *reg_codigo_para_nome(int codigo);
const char *sup_codigo_para_nome(int codigo);

int cond_nome_para_codigo(const char *nome);            // -1 se inválido
const char *cond_codigo_para_nome(int codigo);
bool cond_verdadeira(int codigo, bool n, bool o, bool z, bool c);

// empacota os campos de uma instrução no formato binário indicado
uint16_t instrucao_empacota(int formato, int codop, bool byte_op,
                             int reg, int mod, int dreg,
                             int cond, int im6, int im10);

// instrução decodificada (campos crus + a forma/mnemonico reconhecidos)
typedef struct {
  int formato, codop;
  bool byte_op;
  int reg;   // -1 se não houver campo REG/SUP
  int mod;   // -1 se a instrução não usa modo de endereçamento
  int dreg;
  int cond;  // -1 se não houver campo COND
  int im6;
  int im10;
  const instr_info_t *info; // NULL se opcode não reconhecido (ilegal)
} instr_decod_t;

void instrucao_decodifica(uint16_t palavra, instr_decod_t *out);
bool instrucao_requer_imediato(const instr_decod_t *d);

// gera o texto em linguagem de montagem de uma instrução já decodificada;
// "imediato" só é relevante quando instrucao_requer_imediato() for true
void instrucao_desmonta(const instr_decod_t *d, uint16_t imediato, char *saida, size_t tam);

#endif
