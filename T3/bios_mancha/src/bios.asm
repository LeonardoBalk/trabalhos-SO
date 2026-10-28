; bios.asm -- BIOS mínima para o Mancha completo: vetor de interrupções,
; handlers padrão de exceção, drivers de console/disco/relógio, e um
; mecanismo de despacho de chamadas de sistema ("trap 7"), para servir de
; base a um sistema operacional de brinquedo escrito pelos alunos.
;
; Como usar: monte este arquivo JUNTO com o(s) .asm do kernel (o montador
; do Mancha completo trata vários arquivos passados numa só chamada como
; um módulo só, resolvendo os símbolos entre eles -- ver
; ../simulador_completo/README.md):
;
;   montador src/bios.asm meukernel.asm -o meukernel.mob
;
; O kernel precisa definir um rótulo "_kernel_inicio": é para lá que
; _start salta depois de inicializar a BIOS, com as interrupções externas
; ainda DESLIGADAS (SR.D=1) -- para que a inicialização do kernel (montar
; tabela de processos, tabela de syscalls, etc.) não seja interrompida no
; meio. Ligue com "ei" quando estiver pronto.
;
; Convenção de chamada das rotinas da BIOS: diferente do compilador C do
; Mancha completo (que empilha argumentos), aqui os argumentos vêm em
; r0, r1, r2, r3 (nessa ordem) e o resultado (se houver) volta em r0 --
; mais simples de usar/ler em assembly de mão. r0-r4 são sempre
; considerados "sujos" depois de qualquer "call" (nenhum é preservado).
;
; ================================ interface exportada ===================
;
;   bios_putc                 (r0=caractere)                escreve 1 caractere na console
;   bios_imprime                (r0=endereço de string term. 0)  escreve uma string
;   bios_imprime_dec               (r0=valor com sinal)             escreve um inteiro decimal
;   bios_imprime_hex                (r0=valor)                       escreve 4 dígitos hexadecimais
;   bios_console_disponivel            ()                     r0 = diferente de 0 se há
;                                                                  caractere esperando no buffer
;   bios_console_le                     ()                     r0 = próximo caractere (NÃO
;                                                                  verifica disponibilidade --
;                                                                  chame bios_console_disponivel antes)
;   bios_disco_le_setor                    (r0=face,r1=trilha,r2=setor,r3=endereço destino)
;                                                                  lê 1 setor (512 bytes) do disco 0,
;                                                                  bloqueante (espera terminar)
;   bios_disco_escreve_setor                 (r0=face,r1=trilha,r2=setor,r3=endereço fonte)
;                                                                  escreve 1 setor no disco 0, bloqueante
;   bios_disco_erro                            ()               r0 = diferente de 0 se a última
;                                                                    operação de disco deu erro
;   bios_copia_quadro                            (r0=origem,r1=destino)  copia um quadro de
;                                                                    contexto de 16 palavras (32
;                                                                    bytes) -- ver a seção
;                                                                    "escalonador" do README antes
;                                                                    de escrever um escalonador
;
;   tabela_syscalls (dado, 16 palavras) -- o kernel escreve aqui os
;     endereços dos handlers das suas chamadas de sistema (número da
;     chamada = índice, 0 a 15). Uma entrada em 0 (o padrão, já
;     zerado por _start) significa "não implementada": devolve -1
;     automaticamente. Handlers recebem o número da chamada em r0 e até
;     4 argumentos em r1..r4, devolvem o resultado em r0 e terminam com
;     "ret" -- a BIOS cuida do resto (ver a seção "chamadas de sistema"
;     no README). Chame com "ld r0, <numero>; ld r1, <arg1>; ...; trap 7".
;
;   retomada_escalonador (dado, 1 palavra) -- o kernel escreve aqui o
;     endereço do seu escalonador para ligar multitarefa preemptiva.
;     Esse endereço é alcançado por JMP (não CALL) a cada estouro do
;     relógio, com o quadro de 16 palavras do processo interrompido já
;     empilhado no sp dele -- o escalonador termina escolhendo um
;     processo (possivelmente outro) e executando "rete". 0 (o padrão)
;     significa "sem escalonador": o relógio só conta os tiques e devolve
;     o controle ao mesmo processo.
;
;   contador_tiques (dado, 1 palavra) -- incrementado a cada estouro do
;     relógio, antes de olhar retomada_escalonador; útil para temporizar
;     coisas mesmo sem multitarefa.

        .equ    pilha_sistema = 0xF000
        .equ    pilha_nucleo  = 0xE000

; =========================================================== vetor (0-127)

        .org    0
        .dw     _start,          pilha_sistema, 0, 0   ; 0  início de operação
        .dw     _bios_panico_seg,      pilha_sistema, 0, 0   ; 1  violação de segmento
        .dw     _bios_panico_priv,     pilha_sistema, 0, 0   ; 2  instrução privilegiada
        .dw     _bios_panico_div0,     pilha_sistema, 0, 0   ; 3  divisão por zero
        .dw     _bios_panico_ilegal,   pilha_sistema, 0, 0   ; 4  instrução ilegal
        .dw     _bios_pagina_ausente,  pilha_sistema, 0, 0   ; 5  ausência de quadro (paginação)
        .dw     0, 0, 0, 0                              ; 6  livre
        .dw     _bios_chamada_sistema, pilha_sistema, 0, 0   ; 7  chamada de sistema ("trap 7")
        .dw     _bios_trata_console,   pilha_sistema, 0, 0   ; 8  console
        .dw     _bios_trata_disco,     pilha_sistema, 0, 0   ; 9  disco
        .dw     _bios_trata_relogio,   pilha_sistema, 0, 0   ; 10 relógio
        .dw     0, 0, 0, 0                              ; 11 livre
        .dw     0, 0, 0, 0                              ; 12 livre
        .dw     0, 0, 0, 0                              ; 13 livre
        .dw     0, 0, 0, 0                              ; 14 livre
        .dw     0, 0, 0, 0                              ; 15 livre

; ================================================================= boot

        .org    0x80
_start:
        ; IMPORTANTE: troca a pilha ANTES de fazer qualquer "call" ou
        ; "trap" -- pilha_sistema (a que o boot deixou em sp) é usada
        ; como ponto de REINÍCIO FIXO por TODO quadro do vetor de
        ; interrupções (ver topo do arquivo): a cada interrupção/trap, a
        ; CPU reescreve sp para o valor daquele campo e empilha o quadro
        ; de 16 palavras a partir dali, não importa onde sp estivesse
        ; antes. Se o próprio kernel continuasse rodando em
        ; pilha_sistema, qualquer "trap" (inclusive uma chamada de
        ; sistema feita pelo próprio kernel) reescreveria por cima do
        ; endereço de retorno que ele tivesse empilhado -- pilha_sistema
        ; só é segura para o que empilha e desempilha inteiramente DENTRO
        ; de um único atendimento de interrupção (nunca sobrevive a um
        ; "rete"). Por isso o kernel (e tudo que ele chamar fora de um
        ; handler) roda em pilha_nucleo, uma área totalmente separada.
        ld      sp, pilha_nucleo

        ; zera a tabela de chamadas de sistema (toda entrada "não
        ; implementada" por padrão, até o kernel preencher a sua)
        ld      r0, tabela_syscalls
        ld      r1, 0
_bios_zst_laco:
        cmp     r1, 16
        jmpc    ge, _bios_zst_fim
        ld      r2, 0
        st      r2, (r0)
        add     r0, 2
        add     r1, 1
        jmp     _bios_zst_laco
_bios_zst_fim:
        call    _kernel_inicio
        halt                              ; segurança, caso _kernel_inicio "retorne"

; ============================================== chamada de sistema (trap 7)
;
; Ao entrar aqui, r0 = número da chamada, r1..r4 = argumentos (tudo
; ainda com os valores que o chamador tinha antes do "trap 7" -- a CPU
; não mexe nos registradores de uso geral ao entrar numa interrupção, só
; empilha uma CÓPIA deles). O despacho usa "bp" como registrador
; temporário: como este handler sempre termina em "rete" (que
; restaura TODOS os registradores a partir do quadro empilhado, não do
; que estiver na CPU no momento), não faz mal nenhum sujar "bp" aqui --
; só não dá pra fazer isso em uma função "normal" (com "ret").
_bios_chamada_sistema:
        ld      bp, r0
        cmp     bp, 16
        jmpc    hs, _bios_cs_invalida           ; "hs" (sem sinal) cobre bp<0 e bp>=16 de uma vez
        add     bp, bp                     ; índice * 2 (palavras de 2 bytes)
        add     bp, tabela_syscalls
        ld      bp, (bp)                    ; endereço do handler (ou 0)
        cmp     bp, 0
        jmpc    eq, _bios_cs_invalida
        call    bp                            ; chama o handler (recebe r0..r4, devolve em r0)
        st      r0, (sp)                       ; ver README: "rete" restaura r0 do QUADRO empilhado,
        rete                                     ; não do registrador ao vivo -- por isso gravamos aqui
_bios_cs_invalida:
        ld      r0, -1
        st      r0, (sp)
        rete

; ================================================ handlers de exceção
;
; Todos seguem o mesmo padrão: uma exceção de CPU é sempre um bug (no
; kernel ou em algum processo) -- a BIOS mínima simplesmente imprime o
; que aconteceu e para. Um kernel mais completo ia querer, por exemplo,
; matar só o processo culpado em vez do sistema inteiro; isso fica como
; exercício (o endereço da instrução que causou a exceção está disponível
; em "(sp+14)" logo na entrada do handler -- ver o quadro de 16 palavras
; documentado no README).

_bios_panico_seg:
        ld      r0, _bios_msg_seg
        jmp     _bios_panico_comum
_bios_panico_priv:
        ld      r0, _bios_msg_priv
        jmp     _bios_panico_comum
_bios_panico_div0:
        ld      r0, _bios_msg_div0
        jmp     _bios_panico_comum
_bios_panico_ilegal:
        ld      r0, _bios_msg_ilegal
        jmp     _bios_panico_comum
_bios_pagina_ausente:
        ld      r0, _bios_msg_pagina
        jmp     _bios_panico_comum

_bios_panico_comum:
        call    bios_imprime
        ld      r0, (sp+14)                ; ip da instrução que causou a exceção
        call    bios_imprime_hex
        ld      r0, _bios_texto_nl
        call    bios_imprime
        halt

; ============================================================ relógio

_bios_trata_relogio:
        ld      r0, (contador_tiques)
        add     r0, 1
        st      r0, (contador_tiques)
        ld      r0, (retomada_escalonador)
        cmp     r0, 0
        jmpc    eq, _bios_tr_sem_escalonador
        jmp     r0                          ; não retorna por aqui: o escalonador termina com "rete"
_bios_tr_sem_escalonador:
        rete

; ============================================================= console
;
; O console gera UMA interrupção mesmo que várias teclas cheguem "juntas"
; antes de serem atendidas (o simulador só marca "tem entrada pendente",
; não conta quantas) -- por isso o handler esvazia a porta inteira a cada
; chamada, guardando os caracteres num buffer circular próprio, em vez de
; supor "1 interrupção = 1 caractere".

_bios_trata_console:
        push    bp
        ld      bp, sp
_bios_tc_laco:
        inb     r0, (2)
        and     r0, 2
        cmp     r0, 0
        jmpc    eq, _bios_tc_fim
        inb     r0, (1)
        and     r0, 255
        ld      r1, (_bios_console_buf_n)
        cmp     r1, 32
        jmpc    ge, _bios_tc_laco                 ; buffer cheio: descarta o caractere, tenta o próximo
        ld      r1, (_bios_console_buf_cauda)
        ld      r2, _bios_console_buf
        add     r2, r1
        stb     r0, (r2)
        add     r1, 1
        cmp     r1, 32
        jmpc    lt, _bios_tc_grava_cauda
        ld      r1, 0
_bios_tc_grava_cauda:
        st      r1, (_bios_console_buf_cauda)
        ld      r1, (_bios_console_buf_n)
        add     r1, 1
        st      r1, (_bios_console_buf_n)
        jmp     _bios_tc_laco
_bios_tc_fim:
        ld      sp, bp
        pop     bp
        rete

bios_console_disponivel:
        ld      r0, (_bios_console_buf_n)
        ret

bios_console_le:
        ld      r1, (_bios_console_buf_cabeca)
        ld      r2, _bios_console_buf
        add     r2, r1
        ldb     r0, (r2)
        and     r0, 255
        add     r1, 1
        cmp     r1, 32
        jmpc    lt, _bios_cl_grava_cabeca
        ld      r1, 0
_bios_cl_grava_cabeca:
        st      r1, (_bios_console_buf_cabeca)
        ld      r2, (_bios_console_buf_n)
        add     r2, -1
        st      r2, (_bios_console_buf_n)
        ret

bios_putc:
        outb    r0, (1)
        ret

; ================================================================ disco
;
; Só a unidade 0 é endereçável por este layout de portas (as portas de
; face/trilha/setor/endereço do simulador são fixas na unidade 0,
; independente do campo de unidade do byte de operação -- ver
; ../simulador_completo/src/dispositivos.c). E/S aqui é sempre síncrona
; (a BIOS espera a operação terminar antes de devolver o controle);
; implementar E/S assíncrona de disco (usando a interrupção 9 de verdade,
; suspendendo o processo chamador em vez de girar num laço) é um bom
; exercício de continuação para o kernel.

_bios_trata_disco:
        rete                                ; ver comentário acima: não é preciso fazer nada aqui
                                              ; na versão síncrona -- ponto de extensão para E/S assíncrona

bios_disco_le_setor:
        outb    r0, (0x10)
        outb    r1, (0x11)
        outb    r2, (0x12)
        ld      r0, r3
        ld      r1, r0
        shr     r1, 8
        outb    r1, (0x14)
        outb    r0, (0x15)
        ld      r0, 0                        ; operação=leitura (bits 7:6 = 00), unidade 0
        outb    r0, (0x13)
        jmp     _bios_disco_espera

bios_disco_escreve_setor:
        outb    r0, (0x10)
        outb    r1, (0x11)
        outb    r2, (0x12)
        ld      r0, r3
        ld      r1, r0
        shr     r1, 8
        outb    r1, (0x14)
        outb    r0, (0x15)
        ld      r0, 64                        ; 0x40: operação=escrita (bits 7:6 = 01), unidade 0
        outb    r0, (0x13)
        jmp     _bios_disco_espera

_bios_disco_espera:
        inb     r0, (0x13)
        ld      r1, r0
        and     r0, 1                          ; bit0: ocupado
        cmp     r0, 0
        jmpc    ne, _bios_disco_espera
        and     r1, 2                           ; bit1: erro (lido da MESMA leitura que viu ocupado=0 --
        st      r1, (_bios_disco_ultimo_erro)          ; ler de novo já teria zerado o erro, ver dispositivos.c)
        ret

bios_disco_erro:
        ld      r0, (_bios_disco_ultimo_erro)
        ret

; ============================================ cópia de quadro de contexto
;
; Ajuda a escrever um escalonador preemptivo: copia as 16 palavras (32
; bytes) de um quadro de contexto de "origem" (r0) para "destino" (r1).
; IMPORTANTE (é o detalhe mais sutil desta BIOS -- ver a seção
; "escalonador" do README): TODO vetor de interrupção do Mancha tem um
; endereço de pilha FIXO (o campo "sp" do quadro, ver o topo deste
; arquivo) -- a CPU sempre reinicia sp nesse mesmo endereço antes de
; empilhar o quadro de quem foi interrompido, não importa quem estivesse
; rodando. Ou seja, o quadro do processo interrompido NÃO fica guardado
; "na pilha dele" -- ele aparece sempre no mesmo lugar (pilha_sistema-32)
; e é sobrescrito na próxima interrupção. Por isso um escalonador
; precisa, a cada troca, COPIAR esse quadro para uma área reservada só
; para aquele processo (com bios_copia_quadro), e depois copiar de volta
; o quadro salvo do processo escolhido antes do "rete" -- só trocar "sp"
; não basta aqui.
bios_copia_quadro:
        push    bp
        ld      bp, sp
        ld      r2, 0
_bios_bcq_laco:
        cmp     r2, 16
        jmpc    ge, _bios_bcq_fim
        ld      r3, (r0+)
        st      r3, (r1+)
        add     r2, 1
        jmp     _bios_bcq_laco
_bios_bcq_fim:
        ld      sp, bp
        pop     bp
        ret

; ====================================================== impressão (decimal/hex)

bios_imprime:
        push    bp
        ld      bp, sp
        ld      r1, r0
_bios_bi_laco:
        ldb     r0, (r1+)
        and     r0, 255                        ; "ldb" só troca o byte baixo -- zero-estende à mão
        cmp     r0, 0
        jmpc    eq, _bios_bi_fim
        outb    r0, (1)
        jmp     _bios_bi_laco
_bios_bi_fim:
        ld      sp, bp
        pop     bp
        ret

bios_imprime_hex:
        push    bp
        ld      bp, sp
        ld      r1, r0
        ld      r2, 12
_bios_bh_laco:
        ld      r0, r1
        shr     r0, r2
        and     r0, 15
        cmp     r0, 10
        jmpc    lt, _bios_bh_digito
        add     r0, 55                          ; 'A' - 10
        jmp     _bios_bh_emite
_bios_bh_digito:
        add     r0, 48                            ; '0'
_bios_bh_emite:
        outb    r0, (1)
        add     r2, -4
        cmp     r2, 0
        jmpc    ge, _bios_bh_laco
        ld      sp, bp
        pop     bp
        ret

bios_imprime_dec:
        push    bp
        ld      bp, sp
        sub     sp, 8                              ; buffer para até 6 dígitos (int cabe em 5 + sinal)
        ld      r1, 0                                ; r1 = 1 se negativo
        cmp     r0, 0
        jmpc    ge, _bios_bd_pula_sinal
        ld      r1, 1
        xor     r0, -1
        add     r0, 1                                  ; r0 = -r0 (complemento de dois)
_bios_bd_pula_sinal:
        ld      r4, bp
        add     r4, -8                                  ; r4 = base do buffer (não se mexe mais)
        ld      r2, r4                                    ; r2 = ponteiro de escrita (posinc)
_bios_bd_laco:
        ld      r3, r0                                      ; r3 = dividendo
        div     r0, 10                                       ; r0 = quociente
        push    r0
        mul     r0, 10
        sub     r3, r0                                         ; r3 = resto
        add     r3, 48
        stb     r3, (r2+)
        pop     r0                                               ; r0 = quociente de volta
        cmp     r0, 0
        jmpc    ne, _bios_bd_laco
        cmp     r1, 0
        jmpc    eq, _bios_bd_imprime
        ld      r0, 45                                             ; '-'
        outb    r0, (1)
_bios_bd_imprime:
        cmp     r2, r4
        jmpc    le, _bios_bd_fim
        add     r2, -1
        ldb     r0, (r2)
        and     r0, 255
        outb    r0, (1)
        jmp     _bios_bd_imprime
_bios_bd_fim:
        ld      sp, bp
        pop     bp
        ret

; ==================================================================== dados

        .data

tabela_syscalls:       .ds 16
retomada_escalonador:  .dw 0
contador_tiques:        .dw 0
_bios_disco_ultimo_erro:       .dw 0

_bios_console_buf_cabeca:      .dw 0
_bios_console_buf_cauda:        .dw 0
_bios_console_buf_n:              .dw 0
_bios_console_buf:                  .ds 16     ; 32 bytes (granularidade de ".ds" é em palavras)

_bios_texto_nl:      .db 10, 0
_bios_msg_seg:       .db "EXCECAO: violacao de segmento em ip=", 0
_bios_msg_priv:      .db "EXCECAO: instrucao privilegiada em ip=", 0
_bios_msg_div0:      .db "EXCECAO: divisao por zero em ip=", 0
_bios_msg_ilegal:    .db "EXCECAO: instrucao ilegal em ip=", 0
_bios_msg_pagina:    .db "EXCECAO: ausencia de quadro (pagina) em ip=", 0
