#ifndef COMPAT_MEMSTREAM_H
#define COMPAT_MEMSTREAM_H

#include <stdio.h>
#include <stdlib.h>

#define COMPAT_MAX_STREAMS 8

static struct {
  FILE *f;
  char **buf;
  size_t *tam;
  char *nome;
} compat_streams[COMPAT_MAX_STREAMS];

static inline FILE *open_memstream(char **buf, size_t *tam)
{
  for (int i = 0; i < COMPAT_MAX_STREAMS; i++) {
    if (compat_streams[i].f != NULL) continue;
    char *nome = _tempnam(NULL, "mcc");
    FILE *f = nome != NULL ? fopen(nome, "w+b") : NULL;
    if (f == NULL) {
      free(nome);
      return NULL;
    }
    compat_streams[i].f = f;
    compat_streams[i].buf = buf;
    compat_streams[i].tam = tam;
    compat_streams[i].nome = nome;
    return f;
  }
  return NULL;
}

static inline int compat_fclose(FILE *f)
{
  for (int i = 0; i < COMPAT_MAX_STREAMS; i++) {
    if (compat_streams[i].f != f) continue;
    long n = ftell(f);
    char *buf = malloc(n + 1);
    rewind(f);
    size_t lido = fread(buf, 1, n, f);
    buf[lido] = '\0';
    *compat_streams[i].buf = buf;
    *compat_streams[i].tam = lido;
    int r = fclose(f);
    remove(compat_streams[i].nome);
    free(compat_streams[i].nome);
    compat_streams[i].f = NULL;
    return r;
  }
  return fclose(f);
}

#define fclose compat_fclose

#endif
