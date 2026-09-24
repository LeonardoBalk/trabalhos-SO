; runtime.asm -- ponto de entrada e primitivas de E/S para programas
; compilados pelo mcc, na convenção de chamada usada pelo código gerado
; (parâmetros empilhados da direita para a esquerda antes de "call",
; quadro de pilha com "bp", valor de retorno em r0, ver README.md).
;
; deve ser montado JUNTO com o(s) .asm gerado(s) pelo mcc (o montador do
; Mancha completo trata vários arquivos passados numa só chamada como um
; único módulo -- ver ../simulador_completo/README.md), por exemplo:
;   montador rt/runtime.asm rt/biblioteca.asm programa.asm -o programa.mob

        .equ    pilha_inicial = 0xF000

        .org    0
        .dw     _start, pilha_inicial, 0, 0    ; quadro 0: início de operação

        .org    0x80
_start:
        call    _f_main
        halt                                     ; para de qualquer forma, mesmo se main não usar "return"

; void mancha_out(int porta, int valor)
_f_mancha_out:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)                       ; porta
        ld      r1, r0
        ld      r0, (bp+6)                       ; valor
        out     r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; int mancha_in(int porta)
_f_mancha_in:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)                       ; porta
        ld      r1, r0
        in      r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; void mancha_halt(void)
_f_mancha_halt:
        push    bp
        ld      bp, sp
        halt
        ld      sp, bp
        pop     bp
        ret
