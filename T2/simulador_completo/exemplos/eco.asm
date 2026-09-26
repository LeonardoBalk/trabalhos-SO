; eco.asm -- espera caracteres digitados na console (comando de operador
; E<texto>) e ecoa cada um de volta na tela, usando espera ativa no bit
; "caractere recebido" da porta de estado da console (0002). Nao termina
; sozinho -- use o comando de operador F para sair do simulador.

        .equ    pilha = 0x3F0

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
espera:
        in      r0, (2)                 ; r0 = estado da console
        and     r0, 2                   ; mascara do bit "caractere recebido"
        cmp     r0, 0
        jmpc    z, espera               ; sem caractere ainda: espera de novo

        in      r0, (1)                 ; le o caractere
        out     r0, (1)                 ; ecoa na tela
        jmpq    espera
