        .equ    pilha = 0x3F0

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
        in      r0, (0x0050)   ; a leitura do byte alto gera um novo numero
        shl     r0, 8
        in      r1, (0x0051)
        add     r0, r1
        call    imprime_hex
        ld      r0, 10
        call    putc
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
