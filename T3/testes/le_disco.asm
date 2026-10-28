; lê do disco as páginas de ola_pag e confere com o que o montador gerou

        .equ    DESTINO = 0x8000

_kernel_inicio:
        ld      r0, msg_lendo
        call    bios_imprime

        ld      r0, 0                   ; face
        ld      r1, 0                   ; trilha
        ld      r2, 0                   ; primeira página de código
        ld      r3, DESTINO
        call    bios_disco_le_setor
        call    bios_disco_erro
        cmp     r0, 0
        jmpc    ne, falhou

        ld      r0, (DESTINO)
        call    bios_imprime_hex
        ld      r0, ' '
        call    bios_putc

        ld      r0, 0
        ld      r1, 0
        ld      r2, 1                   ; segunda página de código
        ld      r3, DESTINO
        call    bios_disco_le_setor
        ld      r0, (DESTINO)
        call    bios_imprime_hex
        ld      r0, ' '
        call    bios_putc

        ld      r0, 0
        ld      r1, 0
        ld      r2, 2                   ; página de dados
        ld      r3, DESTINO
        call    bios_disco_le_setor
        ld      r0, DESTINO
        call    bios_imprime

        ld      r0, msg_fim
        call    bios_imprime
        halt

falhou:
        ld      r0, msg_erro
        call    bios_imprime
        halt

        .data
msg_lendo:
        .db     "disco: ", 0
msg_erro:
        .db     "ERRO de disco", 10, 0
msg_fim:
        .db     10, 0
