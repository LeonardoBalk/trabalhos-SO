; multitarefa.asm -- sistema operacional de brinquedo: 3 processos
; alternando por um escalonador round-robin preemptivo, disparado pelo
; relógio (interrupção 10). Cada processo "roda" indefinidamente
; escrevendo sua letra (via uma chamada de sistema, ver ola_syscall.asm
; para o mecanismo isolado) e fazendo uma espera ocupada -- o relógio
; interrompe no meio dessa espera e troca de processo, então a saída sai
; intercalada (ABCABCABC... aproximadamente, dependendo de onde cada
; processo estava quando foi interrompido).
;
; monte com:
;   montador ../src/bios.asm multitarefa.asm -o multitarefa.mob
;
; LEIA O COMENTÁRIO de bios_copia_quadro em ../src/bios.asm antes de
; mexer neste arquivo: todo vetor de interrupção do Mancha tem um
; endereço de pilha FIXO, então o quadro de contexto do processo
; interrompido NÃO fica "guardado na pilha dele" -- ele sempre aparece no
; mesmo lugar (pilha_sistema-32) e seria sobrescrito na próxima
; interrupção. Por isso o escalonador copia esse quadro para uma área
; reservada por processo (quadro_salvo_pN, abaixo) a cada troca, em vez
; de só trocar "sp" como se poderia imaginar à primeira vista.
;
; AVISO sobre TICKS_POR_FATIA: o relógio conta instruções executadas de
; verdade (disp_tick roda a cada cpu_executa_1, inclusive quando essa
; chamada só atendeu uma interrupção -- ver
; ../simulador_completo/src/cpu.c), então o tempo que o PRÓPRIO
; escalonador leva pra rodar também consome do "orçamento" do relógio.
; Este escalonador chama bios_copia_quadro duas vezes (uma pra guardar o
; processo interrompido, outra pra restaurar o escolhido), e cada cópia
; de 16 palavras custa ~100 instruções -- então o escalonador inteiro já
; consome uns 220-250 instruções sozinho. Se TICKS_POR_FATIA for menor
; que isso, o relógio estoura de NOVO ainda dentro do escalonador (a
; interrupção fica pendente, já que D=1 durante o atendimento, e dispara
; imediatamente assim que "rete" restaura D=0) -- o processo escolhido
; nunca chega a executar uma instrução sequer antes de ser trocado de
; novo, e a "multitarefa" trava sempre no mesmo processo. Foi exatamente
; esse bug que apareceu ao escrever este exemplo (com
; TICKS_POR_FATIA=200); mantenha uma folga confortável acima do custo do
; escalonador.

        .equ    N_PROCESSOS     = 3
        .equ    TICKS_POR_FATIA = 4000     ; ver aviso acima antes de diminuir este valor

        .equ    SYS_ESCREVE     = 1

_kernel_inicio:
        ; instala a chamada de sistema usada pelos processos para escrever
        ld      r0, SYS_ESCREVE
        add     r0, r0
        add     r0, tabela_syscalls
        ld      r1, sys_escreve_impl
        st      r1, (r0)

        ; programa o relógio: dispara a cada TICKS_POR_FATIA instruções
        ld      r0, TICKS_POR_FATIA
        ld      r1, r0
        shr     r1, 8
        outb    r1, (0x22)                  ; PORTA_RELOGIO_LIM_ALTO
        outb    r0, (0x23)                   ; PORTA_RELOGIO_LIM_BAIXO

        ; habilita as interrupções de console (bit0) e relógio (bit2) --
        ; ver ../simulador_completo/src/dispositivos.h, tabela 17
        ld      r0, 5
        outb    r0, (0x30)                  ; PORTA_CTRL_INTERRUPCOES

        ; instala o escalonador -- a partir daqui, todo estouro do
        ; relógio vai chamá-lo em vez do comportamento padrão da BIOS
        ld      r0, escalonador
        st      r0, (retomada_escalonador)

        ld      r0, 0
        st      r0, (processo_atual)

        ei                                    ; liga interrupções externas

        ; inicia "manualmente" o processo 0: copia o quadro inicial dele
        ; (já preparado em quadro_salvo_p0, ver os dados no fim do
        ; arquivo) para pilha_sistema-32 -- o mesmo endereço fixo onde
        ; qualquer interrupção/trap empilha seu quadro -- e dá um "rete"
        ; a partir dali. As próximas trocas ficam por conta do
        ; escalonador, chamado pelo relógio.
        ld      sp, pilha_sistema
        add     sp, -32
        ld      r1, processos
        ld      r0, (r1)
        ld      r1, sp
        call    bios_copia_quadro
        rete

; ------------------------------------------------- escalonador round-robin
;
; chega aqui por JMP (não CALL) a partir do handler do relógio na BIOS:
; sp já aponta para pilha_sistema-32, onde a CPU acabou de empilhar o
; quadro de 16 palavras do processo interrompido (ver o comentário grande
; no topo do arquivo, e o de bios_copia_quadro em bios.asm). Esse
; endereço (sp) não muda em nenhum momento desta rotina -- "call" empilha
; e desempilha abaixo dele, de forma balanceada -- por isso não precisa
; ser salvo em lugar nenhum: é só usar "sp" direto sempre que precisar
; dele.
escalonador:
        ; copia o quadro do processo que acabou de ser interrompido para
        ; a área salva DELE (processos[processo_atual])
        ld      r0, sp
        ld      r1, (processo_atual)
        ld      r2, r1
        add     r2, r2
        add     r2, processos
        ld      r1, (r2)
        call    bios_copia_quadro

        ; escolhe o próximo processo, round-robin
        ld      r1, (processo_atual)
        add     r1, 1
        cmp     r1, N_PROCESSOS
        jmpc    lt, esc_indice_ok
        ld      r1, 0
esc_indice_ok:
        st      r1, (processo_atual)

        ; copia a área salva do processo escolhido de volta para
        ; pilha_sistema-32, de onde "rete" vai lê-la
        ld      r2, r1
        add     r2, r2
        add     r2, processos
        ld      r0, (r2)
        ld      r1, sp
        call    bios_copia_quadro

        rete

; ---------------------------------------------------------- chamada de sistema

; recebe r0=número (ignorado), r1=caractere
sys_escreve_impl:
        ld      r0, r1
        call    bios_putc
        ld      r0, 0
        ret

; --------------------------------------------------------------- processos
;
; cada processo é só um laço infinito: escreve sua letra, espera um
; pouco (a espera é onde o relógio normalmente pega ele no meio), repete.

processo0_codigo:
p0_laco:
        ld      r0, SYS_ESCREVE
        ld      r1, 'A'
        trap    7
        call    atraso
        jmp     p0_laco

processo1_codigo:
p1_laco:
        ld      r0, SYS_ESCREVE
        ld      r1, 'B'
        trap    7
        call    atraso
        jmp     p1_laco

processo2_codigo:
p2_laco:
        ld      r0, SYS_ESCREVE
        ld      r1, 'C'
        trap    7
        call    atraso
        jmp     p2_laco

atraso:
        push    bp
        ld      bp, sp
        ld      r0, 3000
atraso_laco:
        add     r0, -1
        cmp     r0, 0
        jmpc    ne, atraso_laco
        ld      sp, bp
        pop     bp
        ret

; ======================================================================= dados

        .data

processo_atual: .dw 0

; processos[i] = endereço da área salva (16 palavras) do processo i
processos:
        .dw     quadro_salvo_p0, quadro_salvo_p1, quadro_salvo_p2

; quadro de 16 palavras na ORDEM que "rete"/bios_copia_quadro usam (do
; endereço mais baixo para o mais alto): r0,r1,r2,r3,r4,bp,sp,ip,
; s0(sr),s1,s2,s3,s4(cs),s5(cl),s6(ds),s7(dl) -- ver README, seção "o
; quadro de 16 palavras". Os valores abaixo são o estado inicial de cada
; processo, na primeira vez que ele roda; depois disso o escalonador
; mantém essas áreas atualizadas a cada troca. sr=0xA000 = S=1
; (supervisor -- este exemplo não usa modo usuário, para focar só no
; escalonador), I=1 (E/S permitida), D=0 (interrompível de novo assim
; que voltar a rodar); cs/cl/ds/dl=0 (sem segmentação -- não importa
; mesmo, já que S=1 ignora limites).
quadro_salvo_p0:
        .dw     0, 0, 0, 0, 0
        .dw     0
        .dw     topo_pilha_p0
        .dw     processo0_codigo
        .dw     0xA000
        .dw     0, 0, 0
        .dw     0, 0, 0, 0

quadro_salvo_p1:
        .dw     0, 0, 0, 0, 0
        .dw     0
        .dw     topo_pilha_p1
        .dw     processo1_codigo
        .dw     0xA000
        .dw     0, 0, 0
        .dw     0, 0, 0, 0

quadro_salvo_p2:
        .dw     0, 0, 0, 0, 0
        .dw     0
        .dw     topo_pilha_p2
        .dw     processo2_codigo
        .dw     0xA000
        .dw     0, 0, 0
        .dw     0, 0, 0, 0

; pilhas de TRABALHO dos processos (onde cada um empilha suas próprias
; chamadas/locais depois de começar a rodar -- região diferente das
; áreas salvas acima, que só guardam os 16 registradores entre trocas):
; 64 bytes cada, mais que suficiente para este exemplo.
        .ds     32
topo_pilha_p0:
        .ds     32
topo_pilha_p1:
        .ds     32
topo_pilha_p2:
