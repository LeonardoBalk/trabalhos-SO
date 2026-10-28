; ponte.asm -- a "ponte" entre duas convenções de chamada diferentes que
; convivem neste exemplo:
;
;  - a convenção da BIOS (bios_mancha/src/bios.asm): argumentos em
;    r0..r3, resultado em r0, sem empilhar nada -- é a que "_kernel_inicio"
;    (chamado por _start) e "retomada_escalonador" (o gancho do relógio)
;    usam.
;  - a convenção do mcc (compilador_c): argumentos empilhados da direita
;    para a esquerda antes do "call", resultado em r0, prólogo/epílogo
;    "push bp / ld bp,sp / ... / ld sp,bp / pop bp / ret" -- é a que TODO
;    o código gerado a partir de kernel.c usa, inclusive escalonador(),
;    inicializa_processos(), processoA(), etc. (ver
;    ../../../compilador_c/README.md, seção "convenção de chamada").
;
; nada disso é exposto na linguagem C do mcc (sem inline assembly, sem
; acesso a porta) -- por isso as poucas coisas que só dá pra fazer em
; assembly (programar o relógio, ligar interrupções, escrever direto
; numa porta, e iniciar o primeiro processo com um "rete" manual) ficam
; TODAS aqui, deliberadamente a MENOR fatia de código possível deste
; exemplo. Diferente de uma versão anterior deste arquivo, NÃO há mais
; nenhum dado estático aqui (nem tabela de processos, nem pilhas, nem
; "processo_atual") -- tudo isso agora mora em kernel.c como variáveis
; globais comuns (processos, processo_atual, pilhas, pontos_entrada),
; graças ao mcc agora suportar ponteiro de função (ver o comentário no
; topo de kernel.c). Este arquivo só referencia os rótulos que o mcc
; gera pra elas ("_g_<nome>" pra globais, "_f_<nome>" pra funções -- ver
; ../../../compilador_c/src/simbolos.c) -- nunca precisa saber o que tem
; dentro.
;
; monte com (ver também o Makefile deste diretório):
;   mcc -Iinc kernel.c -o kernel.asm
;   montador ../../src/bios.asm ponte.asm kernel.asm -o multitarefa_c.mob
;
; "pilha_sistema" e "pilha_nucleo" (usadas abaixo) são símbolos definidos
; em bios.asm, referenciados aqui diretamente -- não duplicamos o valor
; deles como uma nova constante local, para nunca correr o risco de ficar
; dessincronizado se um dia mudarem lá.

        .equ    TICKS_POR_FATIA = 4000    ; ver o aviso sobre isto em ../multitarefa.asm

; ------------------------------------------------------- E/S para o C
;
; void escreve_caractere(int c);  -- 1 argumento, convenção do mcc: fica
; em (bp+4). O nome do rótulo é "_f_escreve_caractere" porque é assim que
; o mcc nomeia toda função C chamada "escreve_caractere" (prefixo "_f_",
; ver ../../../compilador_c/src/simbolos.c) -- é esse rótulo que o
; "call" gerado a partir de kernel.c vai procurar.
_f_escreve_caractere:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        outb    r0, (1)
        ld      sp, bp
        pop     bp
        ret

; ----------------------------------------------------- inicialização
;
; chamada por bios.asm/_start via "call _kernel_inicio" (por isso o
; rótulo tem que ser exatamente este nome, sem prefixo "_f_" -- é a
; convenção da BIOS, não a do mcc). _start já deixou sp em pilha_nucleo
; antes de chamar aqui (ver README do bios_mancha, "pilha_sistema vs.
; pilha_nucleo") -- não precisamos mexer em sp de novo.
_kernel_inicio:
        ; programa o relógio (só dá pra fazer com "outb", que C não tem)
        ld      r0, TICKS_POR_FATIA
        ld      r1, r0
        shr     r1, 8
        outb    r1, (0x22)                  ; PORTA_RELOGIO_LIM_ALTO
        outb    r0, (0x23)                   ; PORTA_RELOGIO_LIM_BAIXO

        ; habilita interrupções de console (bit0) e relógio (bit2)
        ld      r0, 5
        outb    r0, (0x30)                  ; PORTA_CTRL_INTERRUPCOES

        ; instala a ponte pro escalonador em C -- a partir daqui, todo
        ; estouro do relógio vai chamá-la em vez do comportamento padrão
        ; da BIOS
        ld      r0, escalonador_trampolim
        st      r0, (retomada_escalonador)

        ; monta os quadros iniciais dos processos (usando pontos_entrada[],
        ; um array de ponteiros de função -- ver kernel.c) e copia o do
        ; processo 0 pro endereço fixo onde toda interrupção/trap empilha
        ; seu quadro (pilha_sistema-32 -- ver README do bios_mancha)
        ld      r0, pilha_sistema
        add     r0, -32
        push    r0
        call    _f_inicializa_processos
        add     sp, 2

        ei                                    ; liga interrupções externas

        ; "rete" a partir do endereço onde inicializa_processos() acabou
        ; de escrever o quadro do processo 0 -- IMPORTANTE: "rete"
        ; desempilha a partir do registrador "sp" de verdade, não de
        ; qualquer ponteiro usado para escrever os dados, então também
        ; precisamos deixar "sp" apontando pra lá antes (bug real
        ; encontrado ao escrever a primeira versão deste exemplo:
        ; esquecer este "ld sp" faz "rete" ler lixo da pilha do núcleo em
        ; vez do quadro recém-escrito, e o processador reinicia do zero).
        ld      sp, pilha_sistema
        add     sp, -32
        rete

; -------------------------------------------------- ponte pro escalonador em C
;
; instalada em retomada_escalonador -- chega aqui por JMP (não CALL) a
; partir do handler do relógio na BIOS, com sp já apontando para
; pilha_sistema-32, o endereço FIXO onde a CPU acabou de empilhar o
; quadro de 16 palavras do processo interrompido (ver README,
; "pilha_sistema vs. pilha_nucleo"). Só precisamos empilhar os 3
; argumentos na ordem que o mcc espera (direita para a esquerda) e chamar
; escalonador() como uma função C normal -- ela escreve o resultado
; DIRETO nesse mesmo endereço (o parâmetro "quadro"), então quando ela
; retornar o quadro já está com o conteúdo do processo ESCOLHIDO,
; prontinho pro "rete" logo abaixo.
escalonador_trampolim:
        ld      r1, sp                     ; guarda o endereço do quadro ANTES de mexer na pilha
        ld      r0, _g_processo_atual         ; r0 = &processo_atual (pproc, 3º parâmetro --
        push    r0                             ; "ld r0, rotulo" sem parênteses = o ENDEREÇO do
                                                 ; rótulo, não o valor guardado lá; mesma forma
                                                 ; usada por "ld r0, _g_processos" logo abaixo)
        ld      r0, _g_processos               ; processos (2º parâmetro)
        push    r0
        ld      r0, r1                          ; quadro (1º parâmetro)
        push    r0
        call    _f_escalonador
        add     sp, 6
        rete
