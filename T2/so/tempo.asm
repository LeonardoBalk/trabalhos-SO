; struct tempo { int alto; int baixo; }  valor = alto * 10000 + baixo

        .equ    BASE_TEMPO = 10000

; normaliza r2 (alto) e r4 (baixo, entre -10000 e 19999) e grava em (r3)
_tempo_grava:
        cmp     r4, 0
        jmpc    ge, _tempo_nao_negativo
        add     r4, BASE_TEMPO
        add     r2, -1
_tempo_nao_negativo:
        cmp     r4, BASE_TEMPO
        jmpc    lt, _tempo_normal
        sub     r4, BASE_TEMPO
        add     r2, 1
_tempo_normal:
        st      r2, (r3)
        st      r4, (r3+2)
        ld      sp, bp
        pop     bp
        ret

; void t_soma_int(struct tempo *t, int v), v >= 0
_f_t_soma_int:
        push    bp
        ld      bp, sp
        ld      r0, (bp+6)
        ld      r1, r0
        div     r1, BASE_TEMPO
        ld      r2, r1
        mul     r2, BASE_TEMPO
        sub     r0, r2
        ld      r3, (bp+4)
        ld      r2, (r3)
        add     r2, r1
        ld      r4, (r3+2)
        add     r4, r0
        jmp     _tempo_grava

; void t_soma(struct tempo *t, struct tempo *x)
_f_t_soma:
        push    bp
        ld      bp, sp
        ld      r3, (bp+4)
        ld      r1, (bp+6)
        ld      r2, (r3)
        add     r2, (r1)
        ld      r4, (r3+2)
        add     r4, (r1+2)
        jmp     _tempo_grava

; void t_sub(struct tempo *r, struct tempo *x, struct tempo *y)
_f_t_sub:
        push    bp
        ld      bp, sp
        ld      r1, (bp+6)
        ld      r0, (bp+8)
        ld      r2, (r1)
        sub     r2, (r0)
        ld      r4, (r1+2)
        sub     r4, (r0+2)
        ld      r3, (bp+4)
        jmp     _tempo_grava

; void t_acumula(struct tempo *t, struct tempo *desde): t += agora - desde
_f_t_acumula:
        push    bp
        ld      bp, sp
        ld      r3, (bp+4)
        ld      r1, (bp+6)
        ld      r0, _g_agora
        ld      r2, (r3)
        add     r2, (r0)
        sub     r2, (r1)
        ld      r4, (r3+2)
        add     r4, (r0+2)
        sub     r4, (r1+2)
        jmp     _tempo_grava

; int le_contador(void): contador do relógio, relendo se o byte alto mudou
_f_le_contador:
        ld      r1, 0
        inb     r1, (0x20)
        ld      r0, 0
        inb     r0, (0x21)
        ld      r2, 0
        inb     r2, (0x20)
        cmp     r1, r2
        jmpc    eq, _tempo_contador_ok
        ld      r1, r2
        ld      r0, 0
_tempo_contador_ok:
        shl     r1, 8
        add     r0, r1
        ret
