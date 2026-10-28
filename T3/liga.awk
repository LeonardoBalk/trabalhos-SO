# separa os fontes em dois fluxos, código e dados, porque o montador usa um
# contador de endereços único e intercalaria os dois

BEGIN { seg = "T" }
/^[ \t]*\.text[ \t]*$/ { seg = "T"; next }
/^[ \t]*\.data[ \t]*$/ { seg = "D"; next }
/^[ \t]*\.bss[ \t]*$/  { seg = "D"; next }
{ if (seg == "T") print > texto; else print > dados }
