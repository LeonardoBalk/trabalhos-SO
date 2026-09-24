# Makefile de nível superior: compila o simulador do Mancha completo e,
# em seguida, o compilador C (compilador_c/ depende do montador de
# simulador_completo/bin/ para montar seus exemplos em .mob).

.PHONY: all simulador_completo compilador_c clean clean-simulador_completo clean-compilador_c

all: simulador_completo compilador_c

# compilador_c usa ../simulador_completo/bin/montador, então precisa
# rodar depois
simulador_completo:
	$(MAKE) -C simulador_completo all

compilador_c: simulador_completo
	$(MAKE) -C compilador_c all

clean: clean-compilador_c clean-simulador_completo

clean-simulador_completo:
	$(MAKE) -C simulador_completo clean

clean-compilador_c:
	$(MAKE) -C compilador_c clean
