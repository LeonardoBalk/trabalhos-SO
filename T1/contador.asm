; Mostra as instrucoes e os segundos reais a cada segundo, durante 5 segundos.
        .equ    pilha = 0x3F0
        .equ    segundos_alvo = 5

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
        ld      r0, 1
        out     r0, (0x0040)   ; inicia uma nova medicao
        st      r0, (proximo)

espera:
        ld      r0, 2
        out     r0, (0x0040)   ; captura tempo e instrucoes para leitura dos bytes
        in      r0, (0x0049)
        shl     r0, 8
        in      r1, (0x004A)
        add     r0, r1
        ld      r1, (proximo)
        cmp     r0, r1
        jmpc    lo, espera
        st      r0, (segundos)

        ld      r0, 'I'
        call    putc
        ld      r0, '='
        call    putc
        in      r0, (0x0041)
        shl     r0, 8
        in      r1, (0x0042)
        add     r0, r1
        call    imprime_hex
        in      r0, (0x0043)
        shl     r0, 8
        in      r1, (0x0044)
        add     r0, r1
        call    imprime_digitos

        ld      r0, ' '
        call    putc
        ld      r0, 'S'
        call    putc
        ld      r0, '='
        call    putc
        ld      r0, (segundos)
        call    imprime_hex
        ld      r0, 10
        call    putc

        ld      r0, (segundos)
        cmp     r0, segundos_alvo
        jmpc    hs, fim
        add     r0, 1
        st      r0, (proximo)
        bra     espera
fim:
        halt

; r0: caractere a enviar.
putc:
        in      r2, (0x0002)
        and     r2, 1          ; bit 0: console pronta para saida
        jmpc    z, putc
        out     r0, (0x0001)
        ret

; Imprime r0 como 0xNNNN.
imprime_hex:
        push    r3
        push    r4
        push    r0

        ld      r0, '0'
        call    putc
        ld      r0, 'x'
        call    putc

        pop     r0
        pop     r4
        pop     r3
imprime_digitos:
        push    r3
        push    r4
        ld      r4, 4

prox_digito:
        push    r0
        ld      r3, r0
        shr     r3, 12
        cmp     r3, 10
        jmpc    lt, decimal
        add     r3, 55
        bra     converte

decimal:
        add     r3, 48

converte:
        ld      r0, r3
        call    putc
        pop     r0
        shl     r0, 4
        add     r4, -1
        cmp     r4, 0
        jmpc    nz, prox_digito

        pop     r4
        pop     r3
        ret

        .data
segundos: .dw 0
proximo:  .dw 1
