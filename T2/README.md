# T2 - Implementação de processos

Um mini SO para o Mancha, com processos em modo usuário, chamadas de sistema,
bloqueio por E/S, preempção, três escalonadores e métricas. As decisões de
projeto e os resultados estão no [relatório](RELATORIO.md).

O núcleo fica em `so/`: a ponte em assembly com o vetor de interrupções
(`ponte.asm`), o SO em si (`so.c`) e as métricas (`metricas.c` e `tempo.asm`).
Em `usuario/` estão a biblioteca C adaptada para as chamadas de sistema e os
programas: `init`, `cpu_a`, `cpu_b`, `cpu_c`, `interativo`, `leitor` e
`escritor`. As pastas `bios_mancha/`, `compilador_c/` e `simulador_completo/`
são a base fornecida, sem alterações.

Para compilar e rodar no Linux, com GCC e ncurses, a partir da raiz do
repositório:

```sh
bash T2/compilar.sh
./T2/bin/simulador T2/bin/so.mob
```

No Windows, com MSYS2 em `C:\msys64`, pelo PowerShell:

```powershell
& C:\msys64\usr\bin\bash.exe T2/compilar.sh
.\T2\bin\simulador.exe T2\bin\so.mob
```

No simulador, `D0` e Enter deixam na velocidade máxima e `C` e Enter iniciam.
O init cria três processos de cálculo e um interativo que lê três linhas, que
você digita com `E`, por exemplo `Eola`. Quando todos terminam, o init morre e
o SO mostra as métricas.

O escalonador, o quantum (em tiques do relógio) e o período do relógio (em
instruções) são escolhidos na compilação, por variáveis de ambiente. No
Linux, por exemplo, `ESCALONADOR=prioridade QUANTUM=2 PERIODO=5000 bash
T2/compilar.sh`. No PowerShell, `$env:ESCALONADOR = "prioridade"` antes de
chamar o `compilar.sh` (e `Remove-Item Env:ESCALONADOR` para voltar ao
padrão). O escalonador pode ser `simples`, `circular` ou `prioridade`, o
quantum vai de 1 a 30 e o período de 2000 a 30000; sem nada, fica circular
com quantum 2 e período 5000. Abaixo de 3000 o SO passa a maior parte do tempo
tratando o relógio.

O `T2/bin/executa` roda o SO sem a interface, o que ajuda a testar:
`-e 100000:texto` coloca texto no console quando a CPU chegar em 100000
instruções, `-t` marca cada linha de saída com o instante e `-n` limita o
número de instruções. O `bash T2/experimentos.sh` refaz as execuções do
relatório e demora alguns minutos; no PowerShell, rode `chcp 65001` antes para
os acentos dos títulos aparecerem certos.

As chamadas de sistema usam `trap 7`, com o número em `r0`, o argumento em
`r1` e o resultado de volta em `r0`: `SO_LE` (1) devolve o caractere lido,
`SO_ESCREVE` (2) recebe o caractere, `SO_CRIA_PROC` (7) recebe o nome do
programa e devolve o pid, `SO_MATA_PROC` (8) recebe o pid (0 é o próprio
processo) e `SO_ESPERA_PROC` (9) recebe o pid a esperar. As três últimas
devolvem -1 em caso de erro.
