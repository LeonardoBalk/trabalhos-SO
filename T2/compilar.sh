#!/usr/bin/env bash
set -eu

if [ -d /c/msys64/mingw64/bin ]; then
  export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
fi
cd "$(dirname "$0")"

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

ex=bios_mancha/exemplos/multitarefa_c
"./bin/mcc$exe" "$ex/kernel.c" -o bin/multitarefa_c.asm
"./bin/montador$exe" bios_mancha/src/bios.asm "$ex/ponte.asm" bin/multitarefa_c.asm \
  -o bin/multitarefa_c.mob

echo "Executar da raiz: ./T2/bin/simulador$exe T2/bin/multitarefa_c.mob"
