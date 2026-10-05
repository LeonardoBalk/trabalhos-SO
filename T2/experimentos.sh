#!/usr/bin/env bash
set -eu

if [ -d /c/msys64/usr/bin ]; then
  export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
fi
cd "$(dirname "$0")"
exe=""
case "$(uname -s)" in MINGW*|MSYS*) exe=.exe ;; esac

rapida=(-e '100000:um\n' -e '250000:dois\n' -e '400000:tres\n')
lenta=(-e '300000:um\n' -e '1200000:dois\n' -e '2100000:tres\n')

roda() {
  local esc=$1 q=$2 p=$3 cenario=$4
  local -n entradas=$cenario
  ESCALONADOR=$esc QUANTUM=$q PERIODO=$p "$BASH" compilar.sh >/dev/null 2>&1
  "./bin/executa$exe" -n 20000000 "${entradas[@]}" bin/so.mob > bin/saida.txt 2> bin/estat.txt
  awk -v esc="$esc" -v q="$q" -v p="$p" '
    FNR == NR { if (match($0, /\(([0-9.]+)%\)/, m)) so = m[1]; next }
    /^tempo total:/ { total = $3 }
    /^tempo ocioso:/ { ocioso = $3 }
    /^preempcoes:/ { preemp = $2 }
    /relogio=/ { split($0, a, "relogio="); split(a[2], b, " "); relogio = b[1] }
    /^pid programa/ { tab = 1; next }
    tab && $2 ~ /^cpu_/ { ret_cpu += $3; n_cpu++ }
    tab && $2 == "interativo" { ret_int = $3; resp = $11 }
    END {
      printf "| %s | %s | %s | %s | %s | %s | %s | %s%% | %d | %s | %s |\n",
        esc, q, p, total, ocioso, preemp, relogio, so, ret_cpu / n_cpu, ret_int, resp
    }' bin/estat.txt bin/saida.txt
}

cabecalho() {
  echo "| escalonador | quantum | período | tempo total | ocioso | preempções | int. relógio | no SO | retorno médio CPU | retorno interativo | resposta interativo |"
  echo "|---|---|---|---|---|---|---|---|---|---|---|"
}

echo "## Escalonador e quantum (entrada rápida, período 5000)"
echo
cabecalho
for esc in simples circular prioridade; do
  for q in 1 2 4; do
    [ "$esc" = simples ] && [ "$q" != 2 ] && continue
    roda $esc $q 5000 rapida
  done
done

echo
echo "## Período do relógio (entrada rápida, quantum 2)"
echo
cabecalho
for esc in circular prioridade; do
  for p in 3000 5000 10000; do
    roda $esc 2 $p rapida
  done
done

echo
echo "## Entrada lenta (quantum 2, período 5000)"
echo
cabecalho
for esc in simples circular prioridade; do
  roda $esc 2 5000 lenta
done

for esc in circular prioridade; do
  echo
  echo "## Métricas completas: $esc, quantum 2, período 5000, entrada rápida"
  echo
  ESCALONADOR=$esc "$BASH" compilar.sh >/dev/null 2>&1
  echo '```'
  "./bin/executa$exe" "${rapida[@]}" bin/so.mob 2>/dev/null | sed -n '/=====/,$p'
  echo '```'
done

"$BASH" compilar.sh >/dev/null 2>&1
