#include "config.h"

#define MAX_PIDS 20
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
    int entradas[N_ESTADOS];
    int preempcoes;
    int desbloqueado;
    struct tempo desbloqueio;
    struct tempo resposta;
    int n_respostas;
};

struct metrica met[MAX_PIDS];
struct tempo agora;
struct tempo base_relogio;
struct tempo ocioso_total;
struct tempo ocioso_desde;
int ocioso;
int ultimo_contador;
int tique_antecipado;
int n_irq[N_IRQ];
int n_criados;
int n_preempcoes;

void t_zera(struct tempo *t)
{
    t->alto = 0;
    t->baixo = 0;
}

void t_soma_int(struct tempo *t, int v);

void t_soma(struct tempo *t, struct tempo *x);

void t_sub(struct tempo *r, struct tempo *x, struct tempo *y);

void t_acumula(struct tempo *t, struct tempo *desde);

// r = x / n, com n < 327
void t_div(struct tempo *r, struct tempo *x, int n)
{
    int resto;
    int parte;

    r->alto = x->alto / n;
    resto = x->alto % n;
    parte = resto * 100 + x->baixo / 100;
    r->baixo = (parte / n) * 100;
    resto = parte % n;
    parte = resto * 100 + x->baixo % 100;
    r->baixo = r->baixo + parte / n;
}

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

// o contador pode ter zerado com a interrupção do relógio ainda pendente
void met_le_agora(void)
{
    int c;

    c = le_contador();
    if (c < ultimo_contador && !tique_antecipado) {
        t_soma_int(&base_relogio, PERIODO_RELOGIO);
        tique_antecipado = 1;
    }
    ultimo_contador = c;
    agora = base_relogio;
    t_soma_int(&agora, c);
}

void met_inicia(void)
{
    int i;
    for (i = 0; i < MAX_PIDS; i++) {
        met[i].usado = 0;
    }
    for (i = 0; i < N_IRQ; i++) {
        n_irq[i] = 0;
    }
    t_zera(&base_relogio);
    t_zera(&ocioso_total);
    ocioso = 0;
    ultimo_contador = 0;
    tique_antecipado = 0;
    n_criados = 0;
    n_preempcoes = 0;
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
    n_irq[irq] = n_irq[irq] + 1;
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

void met_cria(int pid, int prog)
{
    struct metrica *m;
    int e;

    n_criados = n_criados + 1;
    m = met_de(pid);
    if (m == 0) {
        return;
    }
    m->usado = 1;
    m->vivo = 1;
    m->prog = prog;
    m->estado = LIVRE;
    m->criacao = agora;
    m->desde = agora;
    m->preempcoes = 0;
    m->desbloqueado = 0;
    t_zera(&m->resposta);
    m->n_respostas = 0;
    for (e = 0; e < N_ESTADOS; e++) {
        t_zera(&m->em_estado[e]);
        m->entradas[e] = 0;
    }
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
        m->entradas[novo] = m->entradas[novo] + 1;
    }
    m->estado = novo;
    m->desde = agora;
}

void met_preempcao(int pid)
{
    struct metrica *m;

    n_preempcoes = n_preempcoes + 1;
    m = met_de(pid);
    if (m != 0) {
        m->preempcoes = m->preempcoes + 1;
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
    k_int(n_irq[irq]);
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
    k_int(n_preempcoes);
    k_puts("\ninterrupcoes:      ");
    met_imprime_irq(" sistema=", 7);
    met_imprime_irq(" console=", 8);
    met_imprime_irq(" relogio=", 10);
    k_puts(" excecoes=");
    k_int(n_irq[1] + n_irq[2] + n_irq[3] + n_irq[4] + n_irq[5]);
    k_putc(10);

    k_puts("\npid programa   retorno prmp  npr");
    k_puts("  pronto  nbl bloqueado");
    k_puts("  nex executando");
    k_puts(" resposta\n");
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
        k_int_w(m->preempcoes, 5);
        k_int_w(m->entradas[PRONTO], 5);
        k_tempo(&m->em_estado[PRONTO], 8);
        k_int_w(m->entradas[BLOQUEADO], 5);
        k_tempo(&m->em_estado[BLOQUEADO], 10);
        k_int_w(m->entradas[EXECUTANDO], 5);
        k_tempo(&m->em_estado[EXECUTANDO], 11);
        if (m->n_respostas > 0) {
            t_div(&t, &m->resposta, m->n_respostas);
            k_tempo(&t, 9);
        } else {
            k_puts("        -");
        }
        k_putc(10);
    }
    k_puts("\ntempos em instrucoes; prmp = preempcoes;\n");
    k_puts("npr/nbl/nex = vezes em pronto/bloqueado/executando\n");
}
