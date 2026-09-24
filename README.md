# T1 - Processador Mancha

Entrega do T1 da disciplina ELC1080 - Sistemas Operacionais.

Os arquivos do trabalho estão em `t1/`:

- `t1.asm`: gera e imprime um número aleatório de 16 bits.
- `contador.asm`: mostra instruções executadas e segundos de execução.
- `leitura.asm`: lê 10 valores do dispositivo aleatório e imprime também em ordem crescente.
- `dispositivo.c`: adiciona ao simulador as portas usadas pelos programas.

## Como compilar

Na raiz do repositório, no Linux:

```sh
bash t1/compilar.sh
./t1/bin/simulador t1/t1.mob
```

No Windows, usando MSYS2 em `C:\msys64`:

```powershell
& C:\msys64\usr\bin\bash.exe t1/compilar.sh
.\t1\bin\simulador.exe t1\t1.mob
```

Para executar os outros programas, troque `t1.mob` por `contador.mob` ou
`leitura.mob`.

Dentro do simulador, use `D0` para velocidade máxima e `C` para iniciar.

O texto original de referência do processador Mancha ficou em
`t1/README-mancha.md`.
