; paginacao.asm -- demonstra a memoria virtual paginada (mancha.pdf,
; secao 8).
;
; DETALHE INTERESSANTE (e' o que este exemplo mais ensina, alem da
; paginacao em si): o registrador que guarda a base do segmento de
; codigo em modo segmentacao (cs) e o ponteiro da tabela de paginas em
; modo paginacao (pt) sao o MESMO registrador fisico, s4 (mancha.pdf,
; secao 8: "o registrador de supervisor s4 passa a ser chamado de pt").
; Isso significa que, entre a instrucao que carrega pt com o endereco da
; tabela e a instrucao que liga o bit P (ainda em modo segmentacao!), a
; busca da PROXIMA instrucao ja usa o novo valor de s4 como base de cs --
; um efeito colateral nao obvio da dupla funcao do registrador. A solucao
; adotada aqui e' colocar uma copia da instrucao "lds sr" exatamente no
; endereco fisico para onde essa busca transitoria vai parar (pt + o
; endereco virtual da instrucao), assim ela e' encontrada de qualquer
; jeito.
;
; a tabela de paginas e' escrita diretamente pelo montador (nao
; construida em tempo de execucao): mapeia a pagina de instrucoes 0 no
; quadro 0 (identidade, necessario para a transicao acima) e a pagina de
; dados 128 (enderecos virtuais de dados 0000-01FF) no quadro 16 (fisico
; 2000h-21FFh), fora da area do codigo.

        .equ    tabela_pt = 0x1000

        .org    0
        .dw     inicio, 0x3F0, 0, 0

        .org    0x80
inicio:
        lds     pt, tabela_pt           ; s4 = 1000h -- a partir daqui cs "virou" 1000h

        ; a busca da proxima instrucao (endereco virtual 0084h) vai
        ; efetivamente ocorrer no endereco fisico 1000h+0084h=1084h,
        ; ainda em modo segmentacao (P so' liga DEPOIS que "lds sr" for
        ; executada) -- ver copia mais abaixo, em 1084h

        .org    0x1084
        lds     sr, 0xF000              ; liga o bit P mantendo S, I e D ligados

        .org    0x88                    ; a partir daqui ja' em modo paginado
        ld      r0, 1234
        ld      r2, 0                   ; endereco virtual de dados 0000
        st      r0, (r2)                ; so existe fisicamente gracas a tabela (quadro 16)
        ld      r1, (r2)                ; le de volta

        cmp     r1, 1234
        jmpc    eq, ok
        ldq     r3, -1                  ; falhou
        jmpq    fim
ok:
        ldq     r3, 1                   ; sucesso
fim:
        halt

        .org    0x1000
        .dw     0x8000                  ; pagina de instrucoes 0 -> quadro 0 (escrita, identidade)

        .org    0x1100                  ; entrada da pagina de dados 128 (0x1000 + 128*2)
        .dw     0x8010                  ; pagina de dados 128 -> quadro 16
