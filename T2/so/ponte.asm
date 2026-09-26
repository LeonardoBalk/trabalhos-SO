_kernel_inicio:
        ld      r0, _so_int1
        st      r0, (8)
        ld      r0, _so_int2
        st      r0, (16)
        ld      r0, _so_int3
        st      r0, (24)
        ld      r0, _so_int4
        st      r0, (32)
        ld      r0, _so_int5
        st      r0, (40)
        ld      r0, _so_int7
        st      r0, (56)
        ld      r0, _so_int8
        st      r0, (64)
        ld      r0, _so_int10
        st      r0, (80)

        ld      r0, pilha_sistema
        add     r0, -32
        push    r0
        call    _f_so_inicia
        add     sp, 2
        jmp     _so_despacha

_so_int1:
        ld      r0, 1
        jmp     _so_entrada
_so_int2:
        ld      r0, 2
        jmp     _so_entrada
_so_int3:
        ld      r0, 3
        jmp     _so_entrada
_so_int4:
        ld      r0, 4
        jmp     _so_entrada
_so_int5:
        ld      r0, 5
        jmp     _so_entrada
_so_int7:
        ld      r0, 7
        jmp     _so_entrada
_so_int8:
        ld      r0, 8
        jmp     _so_entrada
_so_int10:
        ld      r0, 10
        jmp     _so_entrada

; quadro do interrompido em pilha_sistema-32; o SO roda na pilha_nucleo
_so_entrada:
        ld      r1, sp
        ld      sp, pilha_nucleo
        push    r1
        push    r0
        call    _f_so_trata_interrupcao
        add     sp, 4
_so_despacha:
        ld      sp, pilha_sistema
        add     sp, -32
        rete

_f_so_ocioso:
        jmp     _f_so_ocioso

; void k_out(int porta, int valor)
_f_k_out:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, (bp+6)
        outb    r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; int k_in(int porta)
_f_k_in:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 0
        inb     r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; void k_para(void)
_f_k_para:
        halt
