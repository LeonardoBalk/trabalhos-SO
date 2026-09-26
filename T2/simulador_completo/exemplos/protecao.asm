; protecao.asm -- demonstra a mudanca de modo supervisor/usuario e a
; protecao por segmentacao (mancha.pdf, secoes 2, 7 e 8).
;
; o codigo supervisor ('inicio') configura os limites de segmento do
; usuario (cs/cl/ds/dl), monta manualmente na pilha um quadro como o que
; a instrucao 'trap'/uma interrupcao deixaria (16 palavras: r0..r7,
; s0..s7, na ordem que 'rete' espera desempilhar) e usa 'rete' para
; "retornar" para o codigo de usuario -- e assim entrar em modo usuario
; (sr com o bit S desligado).
;
; o codigo de usuario tenta executar 'halt', uma instrucao privilegiada.
; isso causa a interrupcao 2 (instrucao privilegiada, mancha.pdf tabela
; 11), que devolve o controle ao supervisor (quadro 2), que imprime
; "PRIV" na console e para de verdade.

        .equ    pilha_s   = 0x3F0
        .equ    pilha_u   = 0x2F0
        .equ    limite_u  = 0x400

        .org    0
        .dw     inicio, pilha_s, 0, 0     ; quadro 0: inicio de operacao

        .org    0x10
        .dw     trata_priv, pilha_s, 0, 0 ; quadro 2: instrucao privilegiada

        .org    0x80
inicio:
        lds     cs, 0
        lds     cl, limite_u
        lds     ds, 0
        lds     dl, limite_u

        ; monta na pilha supervisora um quadro falso, na ordem que 'rete'
        ; espera desempilhar (r0 no topo, s7 no fundo -- empilha-se ao
        ; contrario: s7,s6,...,s0,ip,sp,bp,r4,...,r0)
        pushs   dl                        ; s7
        pushs   ds                        ; s6
        pushs   cl                        ; s5
        pushs   cs                        ; s4
        ldq     r0, 0
        push    r0                        ; s3 = 0
        push    r0                        ; s2 = 0
        push    r0                        ; s1 = 0
        push    r0                        ; s0 = sr do usuario (S=0: modo usuario)

        ldq     r0, usuario
        push    r0                        ; ip do usuario
        ld      r0, pilha_u
        push    r0                        ; sp do usuario
        ldq     r0, 0
        push    r0                        ; bp
        push    r0                        ; r4
        push    r0                        ; r3
        push    r0                        ; r2
        push    r0                        ; r1
        push    r0                        ; r0

        rete                              ; entra em modo usuario em 'usuario'

usuario:
        halt                              ; instrucao privilegiada: causa interrupcao 2
        jmpq    usuario                   ; nunca executado

trata_priv:
        ldq     r0, 'P'
        out     r0, (1)
        ldq     r0, 'R'
        out     r0, (1)
        ldq     r0, 'I'
        out     r0, (1)
        ldq     r0, 'V'
        out     r0, (1)
        ldq     r0, 10
        out     r0, (1)
        halt
