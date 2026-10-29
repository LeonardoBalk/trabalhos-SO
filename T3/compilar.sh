#!/usr/bin/env bash
set -eu

if [ -d /c/msys64/mingw64/bin ]; then
  export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
fi
cd "$(dirname "$0")"

quantum="${QUANTUM:-2}"
periodo="${PERIODO:-5000}"
if ! [[ "$quantum" =~ ^[0-9]+$ ]] || ! [[ "$periodo" =~ ^[0-9]+$ ]]; then
  echo "QUANTUM e PERIODO devem ser inteiros" >&2; exit 1
fi
quantum=$((10#$quantum))
periodo=$((10#$periodo))
if [ "$quantum" -lt 1 ] || [ "$quantum" -gt 30 ]; then
  echo "QUANTUM deve ser um inteiro entre 1 e 30" >&2; exit 1
fi
if [ "$periodo" -lt 3000 ] || [ "$periodo" -gt 30000 ]; then
  echo "PERIODO deve ser um inteiro entre 3000 e 30000" >&2; exit 1
fi

cc="${CC:-gcc}"
src=simulador_completo/src
mcc_src=compilador_c/src
flags=(-Wall -Wextra -std=gnu11 -g -I"$src")
mcc_flags=(-std=gnu11 -Wall -Wextra -g -D_GNU_SOURCE)
curses=(-lncurses)
exe=""
case "$(uname -s)" in
  MINGW*|MSYS*)
    exe=.exe
    flags+=(-static -I/c/msys64/mingw64/include/ncursesw -DNCURSES_STATIC)
    mcc_flags+=(-static -include compat/memstream.h)
    curses=(-L/c/msys64/mingw64/lib -lncursesw)
    ;;
esac

mkdir -p bin
comum=("$src/instrucao.c" "$src/objeto.c" "$src/memoria.c")
"$cc" "${flags[@]}" -o "bin/montador$exe" "$src/montador.c" "${comum[@]}"
"$cc" "${flags[@]}" -o "bin/simulador$exe" \
  "$src/main.c" "$src/cpu.c" "$src/dispositivos.c" "$src/tela.c" "${comum[@]}" \
  "${curses[@]}"
"$cc" "${flags[@]}" -o "bin/executa$exe" executa.c \
  "$src/cpu.c" "$src/dispositivos.c" "${comum[@]}"
"$cc" "${mcc_flags[@]}" -o "bin/mcc$exe" \
  "$mcc_src/pre.c" "$mcc_src/lexer.c" "$mcc_src/tipos.c" \
  "$mcc_src/simbolos.c" "$mcc_src/parser.c" "$mcc_src/main.c"
"$cc" "${flags[@]}" -o "bin/mob2bin$exe" mob2bin.c

ex=bios_mancha/exemplos/multitarefa_c
"./bin/mcc$exe" "$ex/kernel.c" -o bin/multitarefa_c.asm
"./bin/montador$exe" bios_mancha/src/bios.asm "$ex/ponte.asm" bin/multitarefa_c.asm \
  -o bin/multitarefa_c.mob

case "${ESCALONADOR:-circular}" in
  simples) escalonador=1 ;;
  circular) escalonador=2 ;;
  prioridade) escalonador=3 ;;
  *) echo "ESCALONADOR deve ser simples, circular ou prioridade" >&2; exit 1 ;;
esac
printf '#define ESCALONADOR %s\n#define QUANTUM %s\n#define PERIODO_RELOGIO %s\n' \
  "$escalonador" "$quantum" "$periodo" > bin/config.h

# programas de usuário no disco: código nas páginas 0..n-1, dados a partir
# da página pagina_dados
pagina_dados=16
n_pag_dados=3
tam_setor=512

programas=(init escritor leitor cpu_a cpu_b cpu_c interativo
           testa escreve_codigo fora_mapa estoura)

# programa que o SO cria na inicializacao
prog_init="${PROG_INIT:-init}"
achou=0
for p in "${programas[@]}"; do
  [ "$p" = "$prog_init" ] && achou=1
done
if [ "$achou" = 0 ]; then
  echo "PROG_INIT deve ser um dos programas: ${programas[*]}" >&2; exit 1
fi
printf '#define PROG_INIT "%s"\n' "$prog_init" >> bin/config.h

"./bin/mcc$exe" -Iusuario -Ibin usuario/biblioteca.c -o bin/u_biblioteca.asm
"./bin/mcc$exe" -Iusuario -Ibin usuario/carga.c -o bin/u_carga.asm

monta_programa() {
  local nome=$1
  local extra=()
  case "$nome" in cpu_a|cpu_b|cpu_c) extra=(bin/u_carga.asm) ;; esac
  "./bin/mcc$exe" -Iusuario -Ibin "usuario/$nome.c" -o "bin/u_$nome.asm" >/dev/null
  : > "bin/$nome.text.asm"
  : > "bin/$nome.data.asm"
  awk -v texto="bin/$nome.text.asm" -v dados="bin/$nome.data.asm" -f liga.awk \
    usuario/inicio.asm "bin/u_$nome.asm" "${extra[@]}" bin/u_biblioteca.asm usuario/sistema.asm
  {
    echo "        .text"
    cat "bin/$nome.text.asm"
    echo "        .org $((pagina_dados * tam_setor))"
    echo "        .data"
    cat "bin/$nome.data.asm"
  } > "bin/$nome.lig.asm"
  "./bin/montador$exe" "bin/$nome.lig.asm" -o "bin/$nome.mob" >/dev/null
  "./bin/mob2bin$exe" "bin/$nome.mob" -o "bin/$nome.bin" \
    -d "$pagina_dados" -n "$n_pag_dados"
}

setor=0
tabela=""
bins=()
for p in "${programas[@]}"; do
  n_pag_codigo=$(monta_programa "$p")
  bins+=("bin/$p.bin")
  tabela="$tabela    { \"$p\", $setor, $n_pag_codigo },\n"
  setor=$((setor + n_pag_codigo + n_pag_dados))
done
cat "${bins[@]}" > bin/disco.img
if [ "$(wc -c < bin/disco.img)" -ne "$((setor * tam_setor))" ]; then
  echo "imagem de disco com tamanho inesperado" >&2; exit 1
fi

{
  printf '#define N_PROG_DISCO %s\n' "${#programas[@]}"
  printf '#define PAGINA_DADOS %s\n' "$pagina_dados"
  printf '#define N_PAG_DADOS %s\n' "$n_pag_dados"
  printf '\nstruct prog_disco {\n    char *nome;\n'
  printf '    int setor;\n    int n_pag_codigo;\n};\n\n'
  printf 'struct prog_disco progs_disco[N_PROG_DISCO] = {\n'
  printf "$tabela"
  printf '};\n'
} > bin/programas.h

# tempo.asm fica por último para que _fim_so marque mesmo o fim do SO
asm=()
for fonte in so/*.c; do
  saida="bin/$(basename "${fonte%.c}").asm"
  "./bin/mcc$exe" -Ibin "$fonte" -o "$saida"
  asm+=("$saida")
done
"./bin/montador$exe" bios_mancha/src/bios.asm so/ponte.asm \
  "${asm[@]}" so/tempo.asm -o bin/so.mob

echo "Executar da raiz: ./T3/bin/simulador$exe T3/bin/so.mob -D T3/bin/disco.img"
