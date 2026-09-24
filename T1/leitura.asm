; Le 10 numeros do dispositivo aleatorio e imprime antes e depois de ordenar.
        .equ    pilha = 0x3F0
        .equ    n = 10

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
        ld      r3, vetor
        ld      r4, n
coleta:
        in      r0, (0x0050)
        shl     r0, 8
        in      r1, (0x0051)
        add     r0, r1
        st      r0, (r3+)
        add     r4, -1
        cmp     r4, 0
        jmpc    nz, coleta

; Imprime os n valores a partir de "vetor", um por linha.
imprime_bruto:
        ld      r0, 10
        call    putc

        ld      r3, vetor
        ld      r4, n
imp1:
        cmp     r4, 0
        jmpc    z, ordena
        ld      r0, (r3+)
        call    imprime_hex
        ld      r0, 10
        call    putc
        add     r4, -1
        bra     imp1

; Ordena vetor[0..n-1] em ordem crescente (insercao).
ordena:
        ld      r4, 1              ; i = 1

ordena_i:
        cmp     r4, n
        jmpc    ge, imprime_ordenado

        ld      r2, r4
        shl     r2, 1
        add     r2, vetor          ; r2 = &vetor[i]
        ld      r1, (r2)           ; r1 = chave = vetor[i]

        ld      r3, r2             ; r3 = &vetor[j+1] ("buraco"), comeca em &vetor[i]

desloca:
        cmp     r3, vetor
        jmpc    eq, insere         ; chegou ao inicio do vetor

        ld      r0, r3
        add     r0, -2             ; r0 = &vetor[j]
        ld      r2, (r0)           ; r2 = vetor[j]
        cmp     r2, r1
        jmpc    ls, insere         ; compara os valores sem sinal

        st      r2, (r3)           ; vetor[j+1] = vetor[j]
        ld      r3, r0             ; r3 = &vetor[j]; continua para tras
        bra     desloca

insere:
        st      r1, (r3)           ; vetor[j+1] = chave

        add     r4, 1
        bra     ordena_i

; Imprime vetor[0..n-1] (agora ordenado), um por linha.
imprime_ordenado:
        ld      r0, 10
        call    putc

        ld      r3, vetor
        ld      r4, n
imp2:
        cmp     r4, 0
        jmpc    z, fim
        ld      r0, (r3+)
        call    imprime_hex
        ld      r0, 10
        call    putc
        add     r4, -1
        bra     imp2

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
vetor:  .ds     20              ; 10 palavras
