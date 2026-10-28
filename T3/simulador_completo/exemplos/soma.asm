; soma.asm -- exemplo basico: calcula par1+par2 e guarda em 'resultado',
; depois para. Roda inteiramente em modo supervisor (o modo em que o
; processador comeca apos o quadro de boot, ver mancha.pdf secao 7).
; Bom para conferir carga/armazenamento com modo de enderecamento
; absoluto e a instrucao add.

        .equ    pilha = 0x3F0

        .org    0
        .dw     main, pilha, 0, 0      ; quadro 0: inicio de operacao

        .org    0x80
main:
        ld      r0, (par1)
        ld      r1, (par2)
        add     r0, r1                  ; r0 = par1 + par2
        st      r0, (resultado)
        halt

        .data
par1:      .dw   5
par2:      .dw   10
resultado: .dw   0
