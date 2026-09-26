CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -g -D_GNU_SOURCE
SRC = src/pre.c src/lexer.c src/tipos.c src/simbolos.c src/parser.c src/main.c
MONTADOR = ../simulador_completo/bin/montador

EXEMPLOS_C = $(wildcard exemplos/*.c)
EXEMPLOS_MOB = $(EXEMPLOS_C:.c=.mob)

.PRECIOUS: exemplos/%.asm

all: bin/mcc rt/biblioteca.asm $(EXEMPLOS_MOB)

bin/mcc: $(SRC) src/*.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/mcc $(SRC)

rt/biblioteca.asm: rt/biblioteca.c inc/mancha.h bin/mcc
	bin/mcc -Iinc rt/biblioteca.c -o rt/biblioteca.asm

exemplos/%.asm: exemplos/%.c inc/mancha.h bin/mcc
	bin/mcc -Iinc $< -o $@

exemplos/%.mob: exemplos/%.asm rt/runtime.asm rt/biblioteca.asm
	$(MONTADOR) rt/runtime.asm rt/biblioteca.asm $< -o $@

clean:
	rm -f bin/mcc rt/biblioteca.asm exemplos/*.asm exemplos/*.mob

.PHONY: all clean
