// kernel.c -- mesmo exemplo de multitarefa de ../multitarefa.asm (3
// processos round-robin, disparados pelo relógio), mas com o
// ESCALONADOR (e os próprios processos) escritos no subconjunto de C do
// mcc (../../../compilador_c), não em assembly.
//
// A parte que não dá pra escrever em C -- programar as portas do
// relógio/controlador de interrupções, ligar interrupções (a instrução
// "ei"), e dar o primeiro "rete" manual pra iniciar o processo 0 -- fica
// isolada em ponte.asm, deliberadamente a menor fatia de código possível
// deste exemplo. Leia os comentários de ponte.asm: eles explicam a
// "ponte" entre a convenção de chamada da BIOS (argumentos em
// r0..r3) e a convenção de chamada do mcc (argumentos empilhados), que é
// o que torna possível chamar escalonador() -- uma função C comum -- a
// partir do handler de interrupção do relógio.
//
// Agora que o mcc suporta ponteiro de função (ver
// ../../../compilador_c/README.md, "Ponteiros de função"), a tabela de
// pontos de entrada dos processos (pontos_entrada, abaixo) é montada
// aqui, em C, com uma sintaxe direta de array de ponteiros de função --
// ponte.asm não precisa mais conhecer os rótulos internos que o mcc gera
// para processoA/processoB/processoC. Isso NÃO era possível antes: sem
// ponteiro de função, não havia como um programa C obter o endereço de
// uma função como um valor manipulável, e essa montagem ficava presa em
// assembly (ver bios_mancha/README.md, "Escalonador implementado em C",
// para a versão anterior e o motivo da mudança).

#define N_PROCESSOS 3

void escreve_caractere(int c);   // implementada em ponte.asm (outb direto)

// AVISO sobre esta constante: ../multitarefa.asm (versão 100% assembly
// do mesmo exemplo) usa 3000 iterações aqui, cada uma custando só ~3
// instruções (add/cmp/jmpc) escritas à mão. O mcc não faz nenhuma
// otimização (ver ../../../compilador_c/README.md) -- cada "i = i + 1"
// vira uns 6 instruções reais (carrega, empilha, soma, guarda), então o
// MESMO laço custa uns 6x mais aqui. Por isso o número de iterações foi
// reduzido, só para a alternância entre processos continuar visível num
// orçamento de instruções razoável -- não é uma limitação do escalonador
// em si (que troca de processo exatamente do mesmo jeito e com a mesma
// frequência de antes, contada em INSTRUÇÕES, não em "chamadas de
// atraso()").
void atraso(void)
{
    int i;
    i = 0;
    while (i < 600) {
        i = i + 1;
    }
}

void processoA(void)
{
    for (;;) {
        escreve_caractere('A');
        atraso();
    }
}

void processoB(void)
{
    for (;;) {
        escreve_caractere('B');
        atraso();
    }
}

void processoC(void)
{
    for (;;) {
        escreve_caractere('C');
        atraso();
    }
}

// tabela de pontos de entrada: um array de ponteiros de função, um por
// processo -- o nome de cada função, usado sem "()", vale o endereço
// dela (ver README do compilador_c, "Ponteiros de função").
void (*pontos_entrada[N_PROCESSOS])(void) = { processoA, processoB, processoC };

// pilhas de TRABALHO dos processos (onde cada um empilha suas próprias
// chamadas/locais, ex. dentro de atraso()): uma linha de 32 palavras (64
// bytes) por processo, mais que suficiente para este exemplo. O topo
// (de onde a pilha CRESCE PARA BAIXO) é "&pilhas[i][32]" -- um endereço
// "um-passado-do-fim" da linha, válido em C mesmo sem nunca ser lido
// (o mesmo idioma de "ponteiro logo depois do último elemento" usado
// para marcar o fim de um array).
int pilhas[N_PROCESSOS][32];

// tabela achatada de quadros salvos: processos[p*16 + i] = palavra i do
// quadro do processo p -- ver o comentário de escalonador(), mais
// abaixo, para o formato completo. "48" tem que ser um literal (não
// "N_PROCESSOS * 16"): o mcc exige uma constante inteira PURA no
// tamanho de um array, sem avaliar expressões ali (ver README do
// compilador_c, "Declarações e inicializadores") -- 48 = N_PROCESSOS(3)
// vezes 16 palavras por quadro.
int processos[48];
int processo_atual;

// monta o quadro inicial (16 palavras) do processo "i" dentro de
// processos[] -- as 13 palavras que não são mexidas aqui (r0-r4, bp,
// s1-s7 exceto sr) ficam 0, valor com que processos[] já nasce (global
// sem inicializador -- ver README do compilador_c, "Declarações e
// inicializadores"). Isso é exatamente a montagem que, antes do mcc
// suportar ponteiro de função, só dava pra fazer em assembly.
void monta_quadro_inicial(int i)
{
    int base;
    base = i * 16;
    processos[base + 6] = (int) &pilhas[i][32];  // sp: topo da pilha de trabalho do processo
    processos[base + 7] = (int) pontos_entrada[i]; // ip: onde o processo começa a rodar
    processos[base + 8] = 0xA000;                    // sr: S=1 (supervisor), I=1, D=0
}

// chamada por ponte.asm/_kernel_inicio (depois de programar o hardware
// -- relógio, máscara de interrupções -- coisas que só dá pra fazer com
// "outb", que C não tem): monta o quadro inicial dos N_PROCESSOS
// processos e copia o do processo 0 para "quadro_boot", o endereço FIXO
// onde toda interrupção/trap empilha seu quadro (pilha_sistema-32 -- ver
// README do bios_mancha, "pilha_sistema vs. pilha_nucleo") -- é de lá
// que ponte.asm vai retomar a execução com um "rete" manual logo depois
// de chamar esta função.
void inicializa_processos(int *quadro_boot)
{
    int i;
    for (i = 0; i < N_PROCESSOS; i++) {
        monta_quadro_inicial(i);
    }
    processo_atual = 0;
    for (i = 0; i < 16; i++) {
        quadro_boot[i] = processos[i];
    }
}

// O escalonador de verdade -- round-robin, chamado por
// escalonador_trampolim (ponte.asm) a cada estouro do relógio.
//
//   quadro      endereço FIXO onde a CPU acabou de empilhar as 16
//                palavras de contexto do processo interrompido -- ver
//                "pilha_sistema vs. pilha_nucleo" no README do
//                bios_mancha. É o MESMO endereço onde "rete" vai ler o
//                quadro de volta assim que este escalonador retornar --
//                por isso a função grava o resultado direto nele, em vez
//                de "devolver" um quadro por outro meio.
//   processos    endereço do início da tabela "processos[]" acima
//                 (passado por ponteiro em vez de acessado como global
//                 direto porque quem chama -- escalonador_trampolim, em
//                 ponte.asm -- é assembly, e essa é a convenção de
//                 chamada do mcc: argumentos por ponteiro/valor, nunca
//                 acesso direto a um símbolo de outro arquivo).
//   pproc         ponteiro para "processo_atual" -- passada por ponteiro
//                  para o escalonador poder ler E atualizar seu valor.
void escalonador(int *quadro, int *processos_tab, int *pproc)
{
    int atual;
    int i;
    int base;

    // 1. guarda o quadro do processo que acabou de ser interrompido na
    //    área reservada dele
    atual = *pproc;
    base = atual * 16;
    i = 0;
    while (i < 16) {
        processos_tab[base + i] = quadro[i];
        i = i + 1;
    }

    // 2. escolhe o próximo processo, round-robin
    atual = atual + 1;
    if (atual >= N_PROCESSOS) {
        atual = 0;
    }
    *pproc = atual;

    // 3. copia a área salva do processo escolhido de volta para o
    //    endereço fixo -- é daqui que "rete" (em ponte.asm, logo depois
    //    de chamar esta função) vai retomar a execução dele
    base = atual * 16;
    i = 0;
    while (i < 16) {
        quadro[i] = processos_tab[base + i];
        i = i + 1;
    }
}
