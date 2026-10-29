#include "config.h"
#include "programas.h"

#define N_PROC 8

// o DMA do disco e o proprio SO sao de 16 bits, entao so' os primeiros
// 64 KB da memoria de 1 MB sao utilizaveis
#define TAM_PAGINA 512
#define N_ENTRADAS 256
#define METADE_DADOS 128
#define ULTIMO_QUADRO 104

#define PG_AUSENTE 0
#define PG_CODIGO 1
#define PG_DADOS 3

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

#define IRQ_PAGINA 5
#define IRQ_SISTEMA 7
#define IRQ_CONSOLE 8
#define IRQ_DISCO 9
#define IRQ_RELOGIO 10

#define Q_R0 0
#define Q_R1 1
#define Q_SP 6
#define Q_IP 7
#define Q_SR 8
#define Q_S4 12

#define SR_USUARIO 0x4000
#define SR_OCIOSO 0xE000

void k_out(int porta, int valor);
int k_in(int porta);
void k_para(void);
void so_ocioso(void);
void copia_quadro(int *de, int *para);
int le_setor(int face, int trilha, int setor, int destino);
int fim_so(void);

void met_inicia(void);
void met_entrada(int irq);
void met_ocioso(void);
int met_cria(int pid, int prog);
void met_estado(int pid, int novo);
void met_preempcao(int pid);
void met_falta(int pid);
void met_morte(int pid);
void met_imprime(void);

struct processo {
    int pid;
    int estado;
    int motivo;
    int espera;
    int prio;
    int prog;
    int qpt;
    int quadro[16];
};

struct processo tabela[N_PROC];
int quadro_zero[16];
int livre[ULTIMO_QUADRO];
int primeiro_quadro;
int n_livres;
int pt_nucleo;
int atual;
int proximo_pid;
int n_bloq[4];

int fila[N_PROC];
int fila_inicio;
int fila_n;
int quantum_restante;

// usado só pelas mensagens do próprio SO, que podem esperar a console
void k_putc(int c)
{
    while ((k_in(2) & 1) == 0) {
    }
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
    } else {
        v = -v;
    }
    if (v == 0) {
        k_putc('0');
    }
    while (v < 0) {
        buf[i] = '0' - (v % 10);
        i = i + 1;
        v = v / 10;
    }
    while (i > 0) {
        i = i - 1;
        k_putc(buf[i]);
    }
}

int str_igual(char *a, char *b);

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
    int k;
    int a;

    if (fila_n > 0 && fila[fila_inicio] == i) {
        fila_remove_primeiro();
        return;
    }
    a = fila_inicio;
    for (k = 0; k < fila_n && fila[a] != i; k++) {
        a = (a + 1) % N_PROC;
    }
    if (k == fila_n) {
        return;
    }
    for (k = k + 1; k < fila_n; k++) {
        fila[a] = fila[(a + 1) % N_PROC];
        a = (a + 1) % N_PROC;
    }
    fila_n = fila_n - 1;
}

void so_torna_pronto(int i)
{
    met_estado(tabela[i].pid, PRONTO);
    tabela[i].estado = PRONTO;
    fila_insere(i);
}

void quadros_inicia(void)
{
    int i;
    int fim;

    fim = fim_so();
    primeiro_quadro = fim / TAM_PAGINA + 1;
    if (fim < 0 || primeiro_quadro >= ULTIMO_QUADRO) {
        k_puts("SO: nucleo grande demais\n");
        k_para();
    }
    n_livres = 0;
    for (i = 0; i < ULTIMO_QUADRO; i++) {
        if (i < primeiro_quadro) {
            livre[i] = 0;
        } else {
            livre[i] = 1;
            n_livres = n_livres + 1;
        }
    }
}

int aloca_quadro(void)
{
    int i;

    for (i = primeiro_quadro; i < ULTIMO_QUADRO; i++) {
        if (livre[i]) {
            livre[i] = 0;
            n_livres = n_livres - 1;
            return i;
        }
    }
    return -1;
}

void libera_quadro(int q)
{
    if (q >= primeiro_quadro && q < ULTIMO_QUADRO && !livre[q]) {
        livre[q] = 1;
        n_livres = n_livres + 1;
    }
}

int pt_tipo(int e)
{
    return (e >> 14) & 3;
}

int pt_quadro(int e)
{
    return e & 0x1FFF;
}

int pt_monta(int tipo, int q)
{
    return tipo * 16384 + q;
}

void pt_zera(int *pt)
{
    int i;

    for (i = 0; i < N_ENTRADAS; i++) {
        pt[i] = PG_AUSENTE;
    }
}

void pt_libera(int *pt)
{
    int i;

    for (i = 0; i < N_ENTRADAS; i++) {
        if (pt_tipo(pt[i]) != PG_AUSENTE) {
            libera_quadro(pt_quadro(pt[i]));
            pt[i] = PG_AUSENTE;
        }
    }
}

// a CPU nao desliga o bit P ao interromper, entao o SO roda paginado
// quando interrompe um processo; a tabela identidade deixa isso igual a
// segmentacao plana, e o campo cs de cada quadro do vetor aponta para ela
void nucleo_pagina(void)
{
    int i;
    int *pt;
    int *vetor;

    pt_nucleo = aloca_quadro();
    if (pt_nucleo < 0) {
        k_puts("SO: sem quadro para a tabela do nucleo\n");
        k_para();
    }
    pt = (int *) (pt_nucleo * TAM_PAGINA);
    for (i = 0; i < METADE_DADOS; i++) {
        pt[i] = pt_monta(PG_DADOS, i);
        pt[METADE_DADOS + i] = pt_monta(PG_DADOS, i);
    }
    vetor = 0;
    for (i = 1; i < 16; i++) {
        if (vetor[i * 4] != 0) {
            vetor[i * 4 + 2] = (int) pt;
        }
    }
}

int le_pagina(int linear, int q)
{
    int face;
    int trilha;
    int setor;

    setor = linear % 18;
    trilha = (linear / 18) % 40;
    face = linear / 720;
    return le_setor(face, trilha, setor, q * TAM_PAGINA);
}

int mapeia(int *pt, int entrada, int tipo, int linear)
{
    int q;

    q = aloca_quadro();
    if (q < 0) {
        return 0;
    }
    if (le_pagina(linear, q) != 0) {
        libera_quadro(q);
        return 0;
    }
    pt[entrada] = pt_monta(tipo, q);
    return 1;
}

char *so_nome_programa(int prog)
{
    return progs_disco[prog].nome;
}

int busca_programa(char *nome)
{
    int i;
    for (i = 0; i < N_PROG_DISCO; i++) {
        if (str_igual(progs_disco[i].nome, nome)) {
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
    int pg;
    int qpt;
    int n_cod;
    int setor;
    int *pt;
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

    qpt = aloca_quadro();
    if (qpt < 0) {
        return -1;
    }
    pt = (int *) (qpt * TAM_PAGINA);
    pt_zera(pt);

    // com carga por demanda o código fica ausente e a primeira busca de
    // instrução já provoca a falta que o carrega
    n_cod = progs_disco[prog].n_pag_codigo;
    setor = progs_disco[prog].setor;
    if (!CARGA_DEMANDA) {
        for (pg = 0; pg < n_cod; pg++) {
            if (!mapeia(pt, pg, PG_CODIGO, setor + pg)) {
                pt_libera(pt);
                libera_quadro(qpt);
                return -1;
            }
            pt[METADE_DADOS + pg] = pt[pg];
        }
    }
    for (pg = 0; pg < N_PAG_DADOS; pg++) {
        if (!mapeia(pt, METADE_DADOS + PAGINA_DADOS + pg, PG_DADOS,
                    setor + n_cod + pg)) {
            pt_libera(pt);
            libera_quadro(qpt);
            return -1;
        }
    }
    if (!met_cria(proximo_pid, prog)) {
        pt_libera(pt);
        libera_quadro(qpt);
        return -1;
    }

    p = &tabela[i];
    copia_quadro(quadro_zero, p->quadro);
    p->quadro[Q_SP] = (PAGINA_DADOS + N_PAG_DADOS) * TAM_PAGINA - 2;
    p->quadro[Q_IP] = 0;
    p->quadro[Q_SR] = SR_USUARIO;
    p->quadro[Q_S4] = (int) pt;
    p->qpt = qpt;
    p->pid = proximo_pid;
    p->prog = prog;
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
    k_puts("SO: quadros ");
    k_int(primeiro_quadro);
    k_puts("..");
    k_int(ULTIMO_QUADRO - 1);
    k_puts(", livres no fim: ");
    k_int(n_livres);
    k_putc(10);
    met_imprime();
    k_para();
}

void so_desbloqueia(int i, int retorno)
{
    tabela[i].quadro[Q_R0] = retorno;
    n_bloq[tabela[i].motivo] = n_bloq[tabela[i].motivo] - 1;
    so_torna_pronto(i);
}

void so_bloqueia(struct processo *p, int motivo)
{
    met_estado(p->pid, BLOQUEADO);
    p->estado = BLOQUEADO;
    p->motivo = motivo;
    n_bloq[motivo] = n_bloq[motivo] + 1;
}

void so_mata_processo(int i)
{
    int j;

    if (tabela[i].estado == PRONTO) {
        fila_retira(i);
    } else if (tabela[i].estado == BLOQUEADO) {
        n_bloq[tabela[i].motivo] = n_bloq[tabela[i].motivo] - 1;
    }
    met_morte(tabela[i].pid);
    tabela[i].estado = LIVRE;
    if (tabela[i].quadro[Q_S4] != 0) {
        pt_libera((int *) tabela[i].quadro[Q_S4]);
        libera_quadro(tabela[i].qpt);
        tabela[i].quadro[Q_S4] = 0;
    }
    if (i == atual) {
        atual = -1;
    }
    if (tabela[i].pid == 1) {
        so_encerra();
    }
    if (n_bloq[BLOQ_ESPERA] == 0) {
        return;
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

// quem já está bloqueado esperando a console tem a vez
void sc_le(struct processo *p)
{
    if (n_bloq[BLOQ_LE] == 0 && console_tem_entrada()) {
        p->quadro[Q_R0] = k_in(1);
    } else {
        so_bloqueia(p, BLOQ_LE);
    }
}

void sc_escreve(struct processo *p)
{
    if (n_bloq[BLOQ_ESCREVE] == 0 && console_pode_escrever()) {
        k_putc(p->quadro[Q_R1]);
        p->quadro[Q_R0] = 0;
    } else {
        so_bloqueia(p, BLOQ_ESCREVE);
    }
}

// o nome vem como endereço virtual do processo, que o SO não enxerga
// direto: cada byte passa pela tabela de páginas dele
int traduz_dados(struct processo *p, int virt)
{
    int *pt;
    int e;

    pt = (int *) p->quadro[Q_S4];
    if (pt == 0) {
        return 0;
    }
    e = pt[METADE_DADOS + ((virt >> 9) & 127)];
    if (pt_tipo(e) == PG_AUSENTE) {
        return 0;
    }
    return pt_quadro(e) * TAM_PAGINA + (virt & 511);
}

int copia_nome(struct processo *p, int virt, char *dest, int max)
{
    int i;
    int f;
    char *s;

    for (i = 0; i < max - 1; i++) {
        f = traduz_dados(p, virt + i);
        if (f == 0) {
            return 0;
        }
        s = (char *) f;
        dest[i] = *s;
        if (dest[i] == 0) {
            return 1;
        }
    }
    dest[max - 1] = 0;
    return 1;
}

void sc_cria_proc(struct processo *p)
{
    char nome[16];

    if (!copia_nome(p, p->quadro[Q_R1], nome, 16)) {
        p->quadro[Q_R0] = -1;
        return;
    }
    p->quadro[Q_R0] = so_cria_processo(nome);
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

void k_hex(int v)
{
    char *d;
    int i;

    d = "0123456789ABCDEF";
    for (i = 12; i >= 0; i = i - 4) {
        k_putc(d[(v >> i) & 15]);
    }
}

void so_mata_por_falta(char *motivo)
{
    k_puts("\nSO: processo ");
    k_int(tabela[atual].pid);
    k_puts(" morto, ");
    k_puts(motivo);
    k_puts(", ip=");
    k_hex(tabela[atual].quadro[Q_IP]);
    k_putc(10);
    so_mata_processo(atual);
}

// o Mancha não guarda o endereço que faltou. numa falta de busca de
// instrução o ip salvo é esse endereço; numa falta de dados o ip já
// aponta para a instrução seguinte, cuja página está presente. é assim
// que as duas se distinguem
void so_trata_falta(void)
{
    struct processo *p;
    int pg;
    int q;
    int *pt;

    if (atual < 0) {
        k_puts("\nSO: falta de pagina no nucleo\n");
        k_para();
    }
    p = &tabela[atual];
    pt = (int *) p->quadro[Q_S4];
    pg = (p->quadro[Q_IP] >> 9) & 127;

    if (pg >= PAGINA_DADOS && pg < PAGINA_DADOS + N_PAG_DADOS) {
        so_mata_por_falta("executando na area de dados");
        return;
    }
    if (pg >= progs_disco[p->prog].n_pag_codigo) {
        so_mata_por_falta("busca fora do programa");
        return;
    }
    if (pt_tipo(pt[pg]) != PG_AUSENTE) {
        so_mata_por_falta("acesso a dado fora do mapa");
        return;
    }

    q = aloca_quadro();
    if (q < 0) {
        so_mata_por_falta("sem quadro livre");
        return;
    }
    if (le_pagina(progs_disco[p->prog].setor + pg, q) != 0) {
        libera_quadro(q);
        so_mata_por_falta("erro de disco");
        return;
    }
    pt[pg] = pt_monta(PG_CODIGO, q);
    // mesma página também como dado só-leitura: o processo lê o próprio
    // código, mas escrever nele dá interrupção 1 em vez de 5
    pt[METADE_DADOS + pg] = pt[pg];
    met_falta(p->pid);
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
    int console;
    struct processo *p;

    if (n_bloq[BLOQ_LE] == 0 && n_bloq[BLOQ_ESCREVE] == 0) {
        return;
    }
    console = k_in(2);
    if ((n_bloq[BLOQ_LE] == 0 || (console & 2) == 0)
        && (n_bloq[BLOQ_ESCREVE] == 0 || (console & 1) == 0)) {
        return;
    }
    for (i = 0; i < N_PROC; i++) {
        p = &tabela[i];
        if (p->estado != BLOQUEADO) {
            continue;
        }
        if (p->motivo == BLOQ_LE && (console & 2) != 0) {
            so_desbloqueia(i, k_in(1));
            console = k_in(2);
        } else if (p->motivo == BLOQ_ESCREVE && (console & 1) != 0) {
            k_putc(p->quadro[Q_R1]);
            so_desbloqueia(i, 0);
            console = k_in(2);
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
        met_preempcao(tabela[atual].pid);
        so_torna_pronto(atual);
    } else if (atual >= 0) {
        so_atualiza_prio(atual);
    }
    atual = so_escolhe();
    if (atual >= 0) {
        fila_retira(atual);
        met_estado(tabela[atual].pid, EXECUTANDO);
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
    met_ocioso();
    for (i = 0; i < 16; i++) {
        quadro[i] = 0;
    }
    ocioso = so_ocioso;
    quadro[Q_SP] = 0xE000;
    quadro[Q_IP] = (int) ocioso;
    quadro[Q_SR] = SR_OCIOSO;
    quadro[Q_S4] = pt_nucleo * TAM_PAGINA;
}

void so_trata_interrupcao(int irq, int *quadro)
{
    met_entrada(irq);
    so_salva_estado(quadro);
    if (irq == IRQ_SISTEMA) {
        so_trata_chamada();
    } else if (irq == IRQ_RELOGIO) {
        so_trata_relogio();
    } else if (irq == IRQ_PAGINA) {
        so_trata_falta();
    } else if (irq == IRQ_CONSOLE || irq == IRQ_DISCO) {
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
    for (i = 0; i < 4; i++) {
        n_bloq[i] = 0;
    }
    fila_inicio = 0;
    fila_n = 0;
    quadros_inicia();
    nucleo_pagina();
    met_inicia();
    k_out(0x22, PERIODO_RELOGIO >> 8);
    k_out(0x23, PERIODO_RELOGIO & 255);
    k_out(0x30, 5);
    so_cria_processo(PROG_INIT);
    so_escalona();
    so_despacha(quadro);
}
