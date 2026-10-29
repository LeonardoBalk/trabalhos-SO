#include "config.h"

#define MAX_PIDS 50
#define N_IRQ 16
#define N_ESTADOS 4
#define BASE 10000

#define LIVRE 0
#define PRONTO 1
#define BLOQUEADO 2
#define EXECUTANDO 3

int k_in(int porta);
void k_putc(int c);
void k_puts(char *s);
void k_int(int v);
char *so_nome_programa(int prog);

// tempo = alto * 10000 + baixo, em instruções
struct tempo {
    int alto;
    int baixo;
};

struct metrica {
    int usado;
    int prog;
    int vivo;
    int estado;
    struct tempo criacao;
    struct tempo fim;
    struct tempo desde;
    struct tempo em_estado[N_ESTADOS];
    struct tempo entradas[N_ESTADOS];
    struct tempo preempcoes;
    struct tempo faltas;
    int desbloqueado;
    struct tempo desbloqueio;
    struct tempo resposta;
    int n_respostas;
};

struct metrica *met_area(void);
int met_area_tamanho(void);
void k_para(void);

struct metrica *met;
struct tempo agora;
struct tempo base_relogio;
struct tempo ocioso_total;
struct tempo ocioso_desde;
int ocioso;
int ultimo_contador;
int tique_antecipado;
struct tempo n_irq[N_IRQ];
int n_criados;
struct tempo n_preempcoes;
struct tempo n_faltas;

void t_zera(struct tempo *t)
{
    t->alto = 0;
    t->baixo = 0;
}

void t_soma_int(struct tempo *t, int v);

void t_soma(struct tempo *t, struct tempo *x);

void t_sub(struct tempo *r, struct tempo *x, struct tempo *y);

void t_acumula(struct tempo *t, struct tempo *desde);

void t_div(struct tempo *r, struct tempo *x, int n);

void k_tempo(struct tempo *t, int largura)
{
    char buf[12];
    int n;
    int i;
    int v;

    n = 0;
    v = t->baixo;
    for (i = 0; i < 4; i++) {
        if (t->alto == 0 && v == 0 && i > 0) {
            break;
        }
        buf[n] = v % 10 + '0';
        n = n + 1;
        v = v / 10;
    }
    v = t->alto;
    while (v > 0) {
        buf[n] = v % 10 + '0';
        n = n + 1;
        v = v / 10;
    }

    if (largura > 0 && largura - n < 1) {
        k_putc(' ');
    }
    for (i = n; i < largura; i++) {
        k_putc(' ');
    }
    while (n > 0) {
        n = n - 1;
        k_putc(buf[n]);
    }
}

void k_int_w(int v, int largura)
{
    struct tempo t;
    t_zera(&t);
    t_soma_int(&t, v);
    k_tempo(&t, largura);
}

int le_contador(void);

int t_antes(struct tempo *t, int alto, int baixo);

// o contador pode ter zerado com a interrupção do relógio ainda pendente
void met_le_agora(void)
{
    int c;
    int alto;
    int baixo;

    alto = agora.alto;
    baixo = agora.baixo;
    c = le_contador();
    if (c < ultimo_contador && !tique_antecipado) {
        t_soma_int(&base_relogio, PERIODO_RELOGIO);
        tique_antecipado = 1;
    }
    ultimo_contador = c;
    agora = base_relogio;
    t_soma_int(&agora, c);
    // se o SO demora mais que um período, tiques se perdem; o tempo não volta
    while (t_antes(&agora, alto, baixo)) {
        t_soma_int(&base_relogio, PERIODO_RELOGIO);
        t_soma_int(&agora, PERIODO_RELOGIO);
    }
}

void met_inicia(void)
{
    int i;

    met = met_area();
    if (sizeof(struct metrica) * MAX_PIDS > met_area_tamanho()) {
        k_puts("SO: area de metricas pequena demais\n");
        k_para();
    }
    for (i = 0; i < MAX_PIDS; i++) {
        met[i].usado = 0;
    }
    for (i = 0; i < N_IRQ; i++) {
        t_zera(&n_irq[i]);
    }
    t_zera(&base_relogio);
    t_zera(&agora);
    t_zera(&ocioso_total);
    ocioso = 0;
    ultimo_contador = 0;
    tique_antecipado = 0;
    n_criados = 0;
    t_zera(&n_preempcoes);
    t_zera(&n_faltas);
    met_le_agora();
}

void met_entrada(int irq)
{
    if (irq == 10) {
        if (tique_antecipado) {
            tique_antecipado = 0;
        } else {
            t_soma_int(&base_relogio, PERIODO_RELOGIO);
        }
        ultimo_contador = 0;
    }
    met_le_agora();
    t_soma_int(&n_irq[irq], 1);
    if (ocioso) {
        t_acumula(&ocioso_total, &ocioso_desde);
        ocioso = 0;
    }
}

void met_ocioso(void)
{
    ocioso = 1;
    ocioso_desde = agora;
}

struct metrica *met_de(int pid)
{
    if (pid < 1 || pid > MAX_PIDS) {
        return 0;
    }
    return &met[pid - 1];
}

// devolve 0 se não há onde guardar as métricas desse pid
int met_cria(int pid, int prog)
{
    struct metrica *m;
    int e;

    m = met_de(pid);
    if (m == 0) {
        return 0;
    }
    n_criados = n_criados + 1;
    m->usado = 1;
    m->vivo = 1;
    m->prog = prog;
    m->estado = LIVRE;
    m->criacao = agora;
    m->desde = agora;
    t_zera(&m->preempcoes);
    t_zera(&m->faltas);
    m->desbloqueado = 0;
    t_zera(&m->resposta);
    m->n_respostas = 0;
    for (e = 0; e < N_ESTADOS; e++) {
        t_zera(&m->em_estado[e]);
        t_zera(&m->entradas[e]);
    }
    return 1;
}

void met_estado(int pid, int novo)
{
    struct metrica *m;

    m = met_de(pid);
    if (m == 0) {
        return;
    }
    if (m->estado != LIVRE) {
        t_acumula(&m->em_estado[m->estado], &m->desde);
    }
    if (m->estado == BLOQUEADO && novo == PRONTO) {
        m->desbloqueado = 1;
        m->desbloqueio = agora;
    }
    if (novo == EXECUTANDO && m->desbloqueado) {
        t_acumula(&m->resposta, &m->desbloqueio);
        m->n_respostas = m->n_respostas + 1;
        m->desbloqueado = 0;
    }
    if (novo != LIVRE) {
        t_soma_int(&m->entradas[novo], 1);
    }
    m->estado = novo;
    m->desde = agora;
}

void met_preempcao(int pid)
{
    struct metrica *m;

    t_soma_int(&n_preempcoes, 1);
    m = met_de(pid);
    if (m != 0) {
        t_soma_int(&m->preempcoes, 1);
    }
}

void met_falta(int pid)
{
    struct metrica *m;

    t_soma_int(&n_faltas, 1);
    m = met_de(pid);
    if (m != 0) {
        t_soma_int(&m->faltas, 1);
    }
}

void met_morte(int pid)
{
    struct metrica *m;

    met_estado(pid, LIVRE);
    m = met_de(pid);
    if (m != 0) {
        m->vivo = 0;
        m->fim = agora;
    }
}

void met_imprime_irq(char *nome, int irq)
{
    k_puts(nome);
    k_tempo(&n_irq[irq], 0);
}

void met_imprime(void)
{
    int i;
    int e;
    struct metrica *m;
    struct tempo t;

    k_puts("\n==================== metricas ====================\n");
    k_puts("processos criados:  ");
    k_int(n_criados);
    k_puts("\ntempo total:        ");
    k_tempo(&agora, 0);
    k_puts("\ntempo ocioso:       ");
    k_tempo(&ocioso_total, 0);
    k_puts("\npreempcoes:         ");
    k_tempo(&n_preempcoes, 0);
    k_puts("\nfaltas de pagina:   ");
    k_tempo(&n_faltas, 0);
    k_puts("\ninterrupcoes:      ");
    met_imprime_irq(" sistema=", 7);
    met_imprime_irq(" console=", 8);
    met_imprime_irq(" disco=", 9);
    met_imprime_irq(" relogio=", 10);
    k_puts("\nexcecoes:          ");
    met_imprime_irq(" segmento=", 1);
    met_imprime_irq(" privilegiada=", 2);
    met_imprime_irq(" div_zero=", 3);
    met_imprime_irq(" ilegal=", 4);
    met_imprime_irq(" pagina=", 5);
    k_putc(10);

    for (i = 0; i < MAX_PIDS; i++) {
        m = &met[i];
        if (m->usado && m->vivo && m->estado != LIVRE) {
            t_acumula(&m->em_estado[m->estado], &m->desde);
            m->desde = agora;
        }
    }

    k_puts("\npid programa   retorno prmp  npr");
    k_puts("  pronto  nbl bloqueado");
    k_puts("  nex executando");
    k_puts(" resposta  falt\n");
    for (i = 0; i < MAX_PIDS; i++) {
        m = &met[i];
        if (!m->usado) {
            continue;
        }
        k_int_w(i + 1, 3);
        k_putc(' ');
        k_puts(so_nome_programa(m->prog));
        for (e = 0; so_nome_programa(m->prog)[e] != 0; e++) {
        }
        for (; e < 10; e++) {
            k_putc(' ');
        }
        if (m->vivo) {
            k_puts("  (vivo)");
        } else {
            t_sub(&t, &m->fim, &m->criacao);
            k_tempo(&t, 8);
        }
        k_tempo(&m->preempcoes, 5);
        k_tempo(&m->entradas[PRONTO], 5);
        k_tempo(&m->em_estado[PRONTO], 8);
        k_tempo(&m->entradas[BLOQUEADO], 5);
        k_tempo(&m->em_estado[BLOQUEADO], 10);
        k_tempo(&m->entradas[EXECUTANDO], 5);
        k_tempo(&m->em_estado[EXECUTANDO], 11);
        if (m->n_respostas > 0) {
            t_div(&t, &m->resposta, m->n_respostas);
            k_tempo(&t, 9);
        } else {
            k_puts("        -");
        }
        k_tempo(&m->faltas, 6);
        k_putc(10);
    }
    k_puts("\ntempos em instrucoes; prmp = preempcoes;\n");
    k_puts("npr/nbl/nex = vezes em pronto/bloqueado/executando;\n");
    k_puts("falt = faltas de pagina atendidas\n");
}
