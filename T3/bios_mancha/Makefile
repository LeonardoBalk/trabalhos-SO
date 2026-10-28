MONTADOR = ../simulador_completo/bin/montador

EXEMPLOS_ASM = $(wildcard exemplos/*.asm)
EXEMPLOS_MOB = $(EXEMPLOS_ASM:.asm=.mob)

all: $(EXEMPLOS_MOB) multitarefa_c

exemplos/%.mob: exemplos/%.asm src/bios.asm
	$(MONTADOR) src/bios.asm $< -o $@

# exemplo à parte: tem seu próprio Makefile porque precisa compilar
# kernel.c com o mcc (../compilador_c/bin/mcc) antes de montar
multitarefa_c:
	$(MAKE) -C exemplos/multitarefa_c all

clean:
	rm -f exemplos/*.mob
	$(MAKE) -C exemplos/multitarefa_c clean

.PHONY: all clean multitarefa_c
