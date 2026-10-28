// pre.h -- pré-processador de C (subconjunto): comentários, #include,
// #define (objeto e função, sem variádicos, sem # / ##), #undef,
// #ifdef/#ifndef/#if/#elif/#else/#endif (expressão de #if limitada a
// "defined(X)", "!", "&&", "||", "==", "!=" e literais inteiros).
//
// Produz um único buffer de texto expandido, com marcadores de linha no
// formato "#LINHA <n> \"<arquivo>\"" inseridos nas transições de arquivo
// (equivalente simplificado das diretivas de linha do cpp real), para que
// o lexer consiga reportar erros com o arquivo/linha originais mesmo
// depois de expandir #include.

#ifndef PRE_H
#define PRE_H

// dirs_include: vetor de diretórios adicionais de busca para #include
// (terminado com NULL); pode ser vazio.
// devolve buffer alocado com malloc (chamador libera).
char *pre_processa_arquivo(const char *caminho, const char *dirs_include[]);

#endif
