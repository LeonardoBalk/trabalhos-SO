; entrada de todo programa de usuário, no endereço virtual 0

        .text
_inicio:
        call    _f_main
        ld      r0, SO_MATA_PROC
        ld      r1, 0
        trap    7
