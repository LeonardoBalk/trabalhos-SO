; conta.asm -- imprime "5 4 3 2 1 " na console (contagem regressiva),
; usando a porta de dados da console (0001) diretamente com "out".
; Roda em modo supervisor (I=1 automaticamente ligado no boot), por isso
; pode usar "out" sem violacao de instrucao privilegiada.

        .equ    pilha = 0x3F0

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
laco:
        ld      r0, (contador)
        cmp     r0, 0
        jmpc    le, fim

        ldq     r1, 48                  ; '0' em ASCII
        add     r1, r0                  ; r1 = '0' + contador
        out     r1, (1)                 ; imprime o digito

        ldq     r1, 32                  ; espaco
        out     r1, (1)

        ldq     r1, -1
        add     r0, r1                  ; contador -= 1
        st      r0, (contador)

        jmpq    laco
fim:
        halt

        .data
contador: .dw 5
