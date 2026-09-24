# T1 - Processador Mancha

- `t1.asm`: gera um número aleatório de 16 bits e imprime em hexadecimal.
- `contador.asm`: mostra instruções executadas e segundos reais, durante
  cinco segundos. A saída é `I=0x........ S=0x....`.
- `leitura.asm`: lê dez números do dispositivo aleatório e imprime a lista
  original e a lista em ordem crescente.

Os três rodam sem entrada pelo comando `E`. Números aleatórios podem se repetir.

## Compilar e executar

No Linux, instale GCC e ncurses (`build-essential` e `libncurses-dev` no
Debian/Ubuntu). Na raiz do repositório:

```sh
bash compilar.sh
./bin/simulador t1.mob
```

No Windows, use MSYS2 em `C:\msys64`, com GCC e ncurses para MinGW64.
No PowerShell:

```powershell
& C:\msys64\usr\bin\bash.exe compilar.sh
.\bin\simulador.exe t1.mob
```

Troque `t1.mob` por `contador.mob` ou `leitura.mob` para rodar os outros.
Use o simulador de **`bin/`**, que inclui o dispositivo adicional.

No simulador, digite `D0` e Enter para usar a velocidade máxima, depois
`C` e Enter para iniciar. `R` reinicia e `F` sai. O terminal precisa ter
pelo menos 90 colunas e 36 linhas.

Para compilar e testar, acrescente `--test` ao comando de compilação.

## Dispositivo

`dispositivo.c` acrescenta as portas abaixo. Os valores de vários bytes
são lidos do byte mais significativo para o menos significativo.

| Porta | Uso |
|---|---|
| `0x40` | Escrever 1 inicia a medição; escrever 2 captura tempo e instruções. |
| `0x41`–`0x44` | Instruções contadas, em 32 bits. |
| `0x45`–`0x48` | Milissegundos reais decorridos, em 32 bits. |
| `0x49`–`0x4A` | Segundos inteiros da mesma captura, em 16 bits. |
| `0x50`–`0x51` | Número aleatório de 16 bits; ler `0x50` gera o próximo. |

A captura mantém os bytes estáveis durante a leitura. O tempo decorrido
inclui eventuais pausas do operador. A fonte aleatória é o sistema operacional:
`BCryptGenRandom` no Windows e `getrandom` no Linux.
