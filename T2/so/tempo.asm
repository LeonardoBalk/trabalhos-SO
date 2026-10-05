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

; divide r1:r2 (32 bits sem sinal) por r3 (1 a 32767)
; devolve o quociente em r1:r2 e o resto em r0; usa r4 e bp
_tempo_div32:
        ld      r0, 0
        ld      r4, 32
_tempo_d32_laco:
        ld      bp, r1
        and     bp, 0x8000
        add     r2, r2
        addc    r1, r1
        add     r0, r0
        cmp     bp, 0
        jmpc    eq, _tempo_d32_sem_bit
        add     r0, 1
_tempo_d32_sem_bit:
        cmp     r0, r3
        jmpc    lo, _tempo_d32_menor
        sub     r0, r3
        add     r2, 1
_tempo_d32_menor:
        add     r4, -1
        cmp     r4, 0
        jmpc    ne, _tempo_d32_laco
        ret

; void t_div(struct tempo *r, struct tempo *x, int n): r = x / n, n de 1 a 32767
_f_t_div:
        push    bp
        ld      bp, sp
        ld      r0, (bp+6)
        ld      r3, (r0)
        ld      r0, (r0+2)
        push    r0
        ld      r1, 0
        ld      r2, 0
        ld      r4, 0x4000
_tempo_div_mul:
        add     r2, r2
        addc    r1, r1
        ld      r0, r3
        and     r0, r4
        cmp     r0, 0
        jmpc    eq, _tempo_div_sem
        add     r2, BASE_TEMPO
        addc    r1, 0
_tempo_div_sem:
        shr     r4, 1
        cmp     r4, 0
        jmpc    ne, _tempo_div_mul
        pop     r0
        add     r2, r0
        addc    r1, 0
        ld      r3, (bp+8)
        push    bp
        call    _tempo_div32
        ld      r3, BASE_TEMPO
        call    _tempo_div32
        pop     bp
        ld      r3, (bp+4)
        st      r2, (r3)
        st      r0, (r3+2)
        ld      sp, bp
        pop     bp
        ret

; int t_antes(struct tempo *t, int alto, int baixo): 1 se t < alto*10000+baixo
_f_t_antes:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r2, (r1)
        ld      r0, 1
        cmp     r2, (bp+6)
        jmpc    lt, _tempo_antes_fim
        jmpc    gt, _tempo_antes_nao
        ld      r2, (r1+2)
        cmp     r2, (bp+8)
        jmpc    lt, _tempo_antes_fim
_tempo_antes_nao:
        ld      r0, 0
_tempo_antes_fim:
        ld      sp, bp
        pop     bp
        ret

; área das métricas por pid (MAX_PIDS * sizeof(struct metrica) bytes); em C
; um vetor global desse tamanho viraria milhares de linhas .db no montador
        .equ    TAM_AREA_METRICAS = 11600

; struct metrica *met_area(void)
_f_met_area:
        ld      r0, _area_metricas
        ret

; int met_area_tamanho(void)
_f_met_area_tamanho:
        ld      r0, TAM_AREA_METRICAS
        ret

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

        .data
_area_metricas:
        .ds     5800                    ; TAM_AREA_METRICAS / 2 palavras
