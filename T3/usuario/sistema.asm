        .equ    SO_LE          = 1
        .equ    SO_ESCREVE     = 2
        .equ    SO_CRIA_PROC   = 7
        .equ    SO_MATA_PROC   = 8
        .equ    SO_ESPERA_PROC = 9

; int so_le(void)
_f_so_le:
        push    bp
        ld      bp, sp
        ld      r0, SO_LE
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_escreve(int c)
_f_so_escreve:
        push    bp
        ld      bp, sp
        ld      r0, SO_ESCREVE
        ld      r1, (bp+4)
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_cria_proc(char *nome)
_f_so_cria_proc:
        push    bp
        ld      bp, sp
        ld      r0, SO_CRIA_PROC
        ld      r1, (bp+4)
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_mata_proc(int pid), pid 0 = o próprio processo
_f_so_mata_proc:
        push    bp
        ld      bp, sp
        ld      r0, SO_MATA_PROC
        ld      r1, (bp+4)
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_espera_proc(int pid)
_f_so_espera_proc:
        push    bp
        ld      bp, sp
        ld      r0, SO_ESPERA_PROC
        ld      r1, (bp+4)
        trap    7
        ld      sp, bp
        pop     bp
        ret

; retorno da função principal de um processo
_f_so_fim_processo:
        ld      r0, SO_MATA_PROC
        ld      r1, 0
        trap    7
