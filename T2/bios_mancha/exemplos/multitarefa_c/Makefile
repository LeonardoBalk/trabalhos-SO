MCC = ../../../compilador_c/bin/mcc
MONTADOR = ../../../simulador_completo/bin/montador
BIOS = ../../src/bios.asm

all: multitarefa_c.mob

kernel.asm: kernel.c
	$(MCC) kernel.c -o kernel.asm

multitarefa_c.mob: $(BIOS) ponte.asm kernel.asm
	$(MONTADOR) $(BIOS) ponte.asm kernel.asm -o multitarefa_c.mob

clean:
	rm -f kernel.asm multitarefa_c.mob

.PHONY: all clean
