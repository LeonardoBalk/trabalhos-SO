; ola_syscall.asm -- primeiro exemplo de kernel: instala duas chamadas de
; sistema simples e as invoca via "trap 7", sem multitarefa. Serve para
; entender o mecanismo de despacho (tabela_syscalls, convenção de
; argumentos/retorno) isoladamente, antes do exemplo completo com
; escalonador (ver multitarefa.asm).
;
; monte com:
;   montador ../src/bios.asm ola_syscall.asm -o ola_syscall.mob

        .equ    SYS_ESCREVE = 1     ; syscall 1: escreve um caractere (arg em r1)
        .equ    SYS_SOMA    = 2     ; syscall 2: soma dois inteiros (args em r1,r2), devolve em r0

_kernel_inicio:
        ; instala os handlers na tabela da BIOS: tabela_syscalls[N] fica no
        ; endereço "tabela_syscalls + N*2" (2 bytes por entrada)
        ld      r0, SYS_ESCREVE
        add     r0, r0
        add     r0, tabela_syscalls
        ld      r1, sys_escreve_impl
        st      r1, (r0)

        ld      r0, SYS_SOMA
        add     r0, r0
        add     r0, tabela_syscalls
        ld      r1, sys_soma_impl
        st      r1, (r0)

        ; chama a syscall 1 algumas vezes: "trap 7" com r0=numero, r1=arg1
        ld      r0, SYS_ESCREVE
        ld      r1, 'o'
        trap    7
        ld      r0, SYS_ESCREVE
        ld      r1, 'i'
        trap    7
        ld      r0, SYS_ESCREVE
        ld      r1, '!'
        trap    7
        ld      r0, SYS_ESCREVE
        ld      r1, 10
        trap    7

        ; chama a syscall 2 e imprime o resultado (que volta em r0, do jeito
        ; normal -- a BIOS cuida de gravar isso no lugar certo antes do "rete")
        ld      r0, SYS_SOMA
        ld      r1, 40
        ld      r2, 2
        trap    7
        call    bios_imprime_dec
        ld      r0, texto_nl
        call    bios_imprime

        ; chama um numero de syscall nao implementado, so pra mostrar que
        ; devolve -1 em vez de travar o sistema
        ld      r0, 9
        trap    7
        call    bios_imprime_dec
        ld      r0, texto_nl
        call    bios_imprime

        ret

; --------------------------------------------------------- implementação

; recebe: r0=numero da chamada (pode ignorar), r1=caractere
sys_escreve_impl:
        ld      r0, r1
        call    bios_putc
        ld      r0, 0
        ret

; recebe: r0=numero, r1=a, r2=b -- devolve a soma em r0
sys_soma_impl:
        ld      r0, r1
        add     r0, r2
        ret

        .data
texto_nl:       .db 10, 0
