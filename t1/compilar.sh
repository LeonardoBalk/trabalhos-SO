#!/usr/bin/env bash
set -eu

if [ -d /c/msys64/mingw64/bin ]; then
  export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
fi
cd "$(dirname "$0")"

cc="${CC:-gcc}"
src=../simulador_completo/src
flags=(-Wall -Wextra -std=gnu11 -g -I"$src")
libs=()
curses=(-lncurses)
exe=""
case "$(uname -s)" in
  MINGW*|MSYS*)
    exe=.exe
    flags+=(-static -I/c/msys64/mingw64/include/ncursesw -DNCURSES_STATIC)
    libs+=(-lbcrypt)
    curses=(-L/c/msys64/mingw64/lib -lncursesw)
    ;;
esac

mkdir -p bin
comum=("$src/instrucao.c" "$src/objeto.c" "$src/memoria.c")
"$cc" "${flags[@]}" -o "bin/montador$exe" "$src/montador.c" "${comum[@]}"
"$cc" "${flags[@]}" -o "bin/simulador$exe" \
  "$src/main.c" "$src/cpu.c" dispositivo.c "$src/tela.c" "${comum[@]}" \
  "${curses[@]}" "${libs[@]}"

for programa in t1 contador leitura; do
  "./bin/montador$exe" "$programa.asm" -o "$programa.mob"
done

if [ "${1:-}" = --test ]; then
  "$cc" "${flags[@]}" -o "bin/testes$exe" testes.c \
    "$src/cpu.c" dispositivo.c "${comum[@]}" "${libs[@]}"
  "./bin/testes$exe"
fi

echo "Executar da raiz: ./T1/bin/simulador$exe T1/t1.mob"
