#include "config.h"

#define N_PROC 8
#define TAM_PILHA 256
#define N_PROGRAMAS 7

#define ESC_SIMPLES 1
#define ESC_CIRCULAR 2
#define ESC_PRIORIDADE 3

#define LIVRE 0
#define PRONTO 1
#define BLOQUEADO 2
#define EXECUTANDO 3

#define BLOQ_LE 1
#define BLOQ_ESCREVE 2
#define BLOQ_ESPERA 3

#define SO_LE 1
#define SO_ESCREVE 2
#define SO_CRIA_PROC 7
#define SO_MATA_PROC 8
#define SO_ESPERA_PROC 9

#define IRQ_SISTEMA 7
#define IRQ_CONSOLE 8
#define IRQ_RELOGIO 10

#define Q_R0 0
#define Q_R1 1
#define Q_SP 6
#define Q_IP 7
#define Q_SR 8

#define SR_USUARIO 0
#define SR_OCIOSO 0xA000

void k_out(int porta, int valor);
int k_in(int porta);
void k_para(void);
void so_ocioso(void);
void so_fim_processo(void);
void copia_quadro(int *de, int *para);

void init(void);
void escritor(void);
void leitor(void);
void cpu_a(void);
void cpu_b(void);
void cpu_c(void);
void interativo(void);

struct processo {
    int pid;
    int estado;
    int motivo;
    int espera;
    int prio;
    int quadro[16];
};

struct programa {
    char *nome;
    void (*entrada)(void);
};

struct programa programas[N_PROGRAMAS] = {
    { "init", init },
    { "escritor", escritor },
    { "leitor", leitor },
    { "cpu_a", cpu_a },
    { "cpu_b", cpu_b },
    { "cpu_c", cpu_c },
    { "interativo", interativo }
};

struct processo tabela[N_PROC];
int pilhas[N_PROC][TAM_PILHA];
int atual;
int proximo_pid;
int n_bloqueados;

int fila[N_PROC];
int fila_inicio;
int fila_n;
int quantum_restante;

void k_putc(int c)
{
    k_out(1, c);
}

void k_puts(char *s)
{
    while (*s != 0) {
        k_putc(*s);
        s = s + 1;
    }
}

void k_int(int v)
{
    char buf[8];
    int i;

    i = 0;
    if (v < 0) {
        k_putc('-');
        v = -v;
    }
    if (v == 0) {
        k_putc('0');
    }
    while (v > 0) {
        buf[i] = (v % 10) + '0';
        i = i + 1;
        v = v / 10;
    }
    while (i > 0) {
        i = i - 1;
        k_putc(buf[i]);
    }
}

int str_igual(char *a, char *b)
{
    while (*a != 0 && *a == *b) {
        a = a + 1;
        b = b + 1;
    }
    return *a == *b;
}

void fila_insere(int i)
{
    fila[(fila_inicio + fila_n) % N_PROC] = i;
    fila_n = fila_n + 1;
}

int fila_remove_primeiro(void)
{
    int i;
    i = fila[fila_inicio];
    fila_inicio = (fila_inicio + 1) % N_PROC;
    fila_n = fila_n - 1;
    return i;
}

void fila_retira(int i)
{
    int n;
    int k;
    int x;

    if (fila_n > 0 && fila[fila_inicio] == i) {
        fila_remove_primeiro();
        return;
    }
    n = fila_n;
    for (k = 0; k < n; k++) {
        x = fila_remove_primeiro();
        if (x != i) {
            fila_insere(x);
        }
    }
}

void so_torna_pronto(int i)
{
    tabela[i].estado = PRONTO;
    fila_insere(i);
}

int busca_programa(char *nome)
{
    int i;
    for (i = 0; i < N_PROGRAMAS; i++) {
        if (str_igual(programas[i].nome, nome)) {
            return i;
        }
    }
    return -1;
}

int busca_pid(int pid)
{
    int i;
    for (i = 0; i < N_PROC; i++) {
        if (tabela[i].estado != LIVRE && tabela[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int so_cria_processo(char *nome)
{
    int prog;
    int i;
    int j;
    void (*fim)(void);
    struct processo *p;

    prog = busca_programa(nome);
    if (prog < 0) {
        return -1;
    }
    for (i = 0; i < N_PROC; i++) {
        if (tabela[i].estado == LIVRE) {
            break;
        }
    }
    if (i == N_PROC) {
        return -1;
    }

    p = &tabela[i];
    for (j = 0; j < 16; j++) {
        p->quadro[j] = 0;
    }
    fim = so_fim_processo;
    pilhas[i][TAM_PILHA - 1] = (int) fim;
    p->quadro[Q_SP] = (int) &pilhas[i][TAM_PILHA - 1];
    p->quadro[Q_IP] = (int) programas[prog].entrada;
    p->quadro[Q_SR] = SR_USUARIO;
    p->pid = proximo_pid;
    proximo_pid = proximo_pid + 1;
    p->prio = 500;
    so_torna_pronto(i);
    return p->pid;
}

void so_encerra(void)
{
    int i;

    k_puts("\nSO: init terminou, fim da execucao\n");
    k_puts("SO: processos ainda vivos:");
    for (i = 0; i < N_PROC; i++) {
        if (tabela[i].estado != LIVRE) {
            k_putc(' ');
            k_int(tabela[i].pid);
        }
    }
    k_putc(10);
    k_para();
}

void so_desbloqueia(int i, int retorno)
{
    tabela[i].quadro[Q_R0] = retorno;
    n_bloqueados = n_bloqueados - 1;
    so_torna_pronto(i);
}

void so_bloqueia(struct processo *p, int motivo)
{
    p->estado = BLOQUEADO;
    p->motivo = motivo;
    n_bloqueados = n_bloqueados + 1;
}

void so_mata_processo(int i)
{
    int j;

    if (tabela[i].estado == PRONTO) {
        fila_retira(i);
    } else if (tabela[i].estado == BLOQUEADO) {
        n_bloqueados = n_bloqueados - 1;
    }
    tabela[i].estado = LIVRE;
    if (i == atual) {
        atual = -1;
    }
    if (tabela[i].pid == 1) {
        so_encerra();
    }
    for (j = 0; j < N_PROC; j++) {
        if (tabela[j].estado == BLOQUEADO && tabela[j].motivo == BLOQ_ESPERA
            && tabela[j].espera == tabela[i].pid) {
            so_desbloqueia(j, 0);
        }
    }
}

void so_salva_estado(int *quadro)
{
    if (atual >= 0) {
        copia_quadro(quadro, tabela[atual].quadro);
    }
}

int console_tem_entrada(void)
{
    return (k_in(2) & 2) != 0;
}

int console_pode_escrever(void)
{
    return (k_in(2) & 1) != 0;
}

void sc_le(struct processo *p)
{
    if (console_tem_entrada()) {
        p->quadro[Q_R0] = k_in(1);
    } else {
        so_bloqueia(p, BLOQ_LE);
    }
}

void sc_escreve(struct processo *p)
{
    if (console_pode_escrever()) {
        k_putc(p->quadro[Q_R1]);
        p->quadro[Q_R0] = 0;
    } else {
        so_bloqueia(p, BLOQ_ESCREVE);
    }
}

void sc_cria_proc(struct processo *p)
{
    p->quadro[Q_R0] = so_cria_processo((char *) p->quadro[Q_R1]);
}

void sc_mata_proc(struct processo *p)
{
    int pid;
    int i;

    pid = p->quadro[Q_R1];
    if (pid == 0) {
        pid = p->pid;
    }
    i = busca_pid(pid);
    if (i < 0) {
        p->quadro[Q_R0] = -1;
        return;
    }
    p->quadro[Q_R0] = 0;
    so_mata_processo(i);
}

void sc_espera_proc(struct processo *p)
{
    int pid;

    pid = p->quadro[Q_R1];
    if (pid == p->pid || busca_pid(pid) < 0) {
        p->quadro[Q_R0] = -1;
        return;
    }
    p->espera = pid;
    so_bloqueia(p, BLOQ_ESPERA);
}

void so_trata_chamada(void)
{
    struct processo *p;
    int id;

    if (atual < 0) {
        return;
    }
    p = &tabela[atual];
    id = p->quadro[Q_R0];
    if (id == SO_LE) {
        sc_le(p);
    } else if (id == SO_ESCREVE) {
        sc_escreve(p);
    } else if (id == SO_CRIA_PROC) {
        sc_cria_proc(p);
    } else if (id == SO_MATA_PROC) {
        sc_mata_proc(p);
    } else if (id == SO_ESPERA_PROC) {
        sc_espera_proc(p);
    } else {
        p->quadro[Q_R0] = -1;
    }
}

void so_trata_relogio(void)
{
    if (atual >= 0 && quantum_restante > 0) {
        quantum_restante = quantum_restante - 1;
    }
}

void so_trata_excecao(int irq)
{
    if (atual < 0) {
        k_puts("\nSO: excecao no nucleo\n");
        k_para();
    }
    k_puts("\nSO: processo ");
    k_int(tabela[atual].pid);
    k_puts(" morto pela excecao ");
    k_int(irq);
    k_putc(10);
    so_mata_processo(atual);
}

void so_trata_pendencias(void)
{
    int i;
    struct processo *p;

    if (n_bloqueados == 0) {
        return;
    }
    for (i = 0; i < N_PROC; i++) {
        p = &tabela[i];
        if (p->estado != BLOQUEADO) {
            continue;
        }
        if (p->motivo == BLOQ_LE && console_tem_entrada()) {
            so_desbloqueia(i, k_in(1));
        } else if (p->motivo == BLOQ_ESCREVE && console_pode_escrever()) {
            k_putc(p->quadro[Q_R1]);
            so_desbloqueia(i, 0);
        }
    }
}

int escolhe_simples(void)
{
    int i;
    for (i = 0; i < N_PROC; i++) {
        if (tabela[i].estado == PRONTO) {
            return i;
        }
    }
    return -1;
}

int escolhe_circular(void)
{
    if (fila_n == 0) {
        return -1;
    }
    return fila[fila_inicio];
}

int escolhe_prioridade(void)
{
    int k;
    int i;
    int melhor;

    melhor = -1;
    for (k = 0; k < fila_n; k++) {
        i = fila[(fila_inicio + k) % N_PROC];
        if (melhor < 0 || tabela[i].prio < tabela[melhor].prio) {
            melhor = i;
        }
    }
    return melhor;
}

int so_escolhe(void)
{
    if (ESCALONADOR == ESC_SIMPLES) {
        return escolhe_simples();
    }
    if (ESCALONADOR == ESC_PRIORIDADE) {
        return escolhe_prioridade();
    }
    return escolhe_circular();
}

void so_atualiza_prio(int i)
{
    int t_exec;

    t_exec = QUANTUM - quantum_restante;
    tabela[i].prio = (tabela[i].prio + t_exec * 1000 / QUANTUM) / 2;
}

void so_escalona(void)
{
    if (atual >= 0 && tabela[atual].estado == EXECUTANDO) {
        if (ESCALONADOR == ESC_SIMPLES || quantum_restante > 0) {
            return;
        }
        so_atualiza_prio(atual);
        so_torna_pronto(atual);
    } else if (atual >= 0) {
        so_atualiza_prio(atual);
    }
    atual = so_escolhe();
    if (atual >= 0) {
        fila_retira(atual);
        tabela[atual].estado = EXECUTANDO;
        quantum_restante = QUANTUM;
    }
}

void so_despacha(int *quadro)
{
    void (*ocioso)(void);
    int i;

    if (atual >= 0) {
        copia_quadro(tabela[atual].quadro, quadro);
        return;
    }
    for (i = 0; i < 16; i++) {
        quadro[i] = 0;
    }
    ocioso = so_ocioso;
    quadro[Q_SP] = 0xE000;
    quadro[Q_IP] = (int) ocioso;
    quadro[Q_SR] = SR_OCIOSO;
}

void so_trata_interrupcao(int irq, int *quadro)
{
    so_salva_estado(quadro);
    if (irq == IRQ_SISTEMA) {
        so_trata_chamada();
    } else if (irq == IRQ_RELOGIO) {
        so_trata_relogio();
    } else if (irq == IRQ_CONSOLE) {
    } else {
        so_trata_excecao(irq);
    }
    so_trata_pendencias();
    so_escalona();
    so_despacha(quadro);
}

void so_inicia(int *quadro)
{
    int i;

    for (i = 0; i < N_PROC; i++) {
        tabela[i].estado = LIVRE;
    }
    atual = -1;
    proximo_pid = 1;
    n_bloqueados = 0;
    fila_inicio = 0;
    fila_n = 0;
    k_out(0x22, PERIODO_RELOGIO >> 8);
    k_out(0x23, PERIODO_RELOGIO & 255);
    k_out(0x30, 5);
    so_cria_processo("init");
    so_escalona();
    so_despacha(quadro);
}
