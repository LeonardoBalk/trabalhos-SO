// Reaproveita os dispositivos originais e acrescenta as portas do T1.
#define disp_cria disp_cria_original
#define disp_destroi disp_destroi_original
#define disp_le_byte disp_le_byte_original
#define disp_escreve_byte disp_escreve_byte_original
#define disp_tick disp_tick_original
#include "../simulador_completo/src/dispositivos.c"
#undef disp_cria
#undef disp_destroi
#undef disp_le_byte
#undef disp_escreve_byte
#undef disp_tick

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <errno.h>
#include <time.h>
#include <sys/random.h>
#endif

typedef struct estado_t1 {
  disp_t *disp;
  uint32_t instrucoes, instrucoes_lidas, milissegundos_lidos;
  uint16_t aleatorio;
  double inicio;
  bool medindo;
  struct estado_t1 *proximo;
} estado_t1;

static estado_t1 *estados;

static double agora(void)
{
#ifdef _WIN32
  LARGE_INTEGER contador, frequencia;
  if (!QueryPerformanceCounter(&contador) || !QueryPerformanceFrequency(&frequencia)) abort();
  return (double)contador.QuadPart / (double)frequencia.QuadPart;
#else
  struct timespec instante;
  if (clock_gettime(CLOCK_MONOTONIC, &instante) != 0) abort();
  return instante.tv_sec + instante.tv_nsec / 1e9;
#endif
}

static uint16_t sorteia(void)
{
  uint16_t valor;
#ifdef _WIN32
  if (BCryptGenRandom(NULL, (PUCHAR)&valor, sizeof valor,
                     BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) abort();
#else
  size_t lidos = 0;
  while (lidos < sizeof valor) {
    ssize_t n = getrandom((char *)&valor + lidos, sizeof valor - lidos, 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) abort();
    lidos += (size_t)n;
  }
#endif
  return valor;
}

static estado_t1 *estado(disp_t *disp)
{
  for (estado_t1 *e = estados; e != NULL; e = e->proximo)
    if (e->disp == disp) return e;
  abort();
}

disp_t *disp_cria(mem_t *mem)
{
  disp_t *disp = disp_cria_original(mem);
  estado_t1 *e = calloc(1, sizeof *e);
  if (e == NULL) abort();
  e->disp = disp;
  e->proximo = estados;
  estados = e;
  return disp;
}

void disp_destroi(disp_t *disp)
{
  estado_t1 **p = &estados;
  while (*p != NULL && (*p)->disp != disp) p = &(*p)->proximo;
  if (*p != NULL) {
    estado_t1 *e = *p;
    *p = e->proximo;
    free(e);
  }
  disp_destroi_original(disp);
}

void disp_tick(disp_t *disp)
{
  disp_tick_original(disp);
  estado_t1 *e = estado(disp);
  if (e->medindo) e->instrucoes++;
}

void disp_escreve_byte(disp_t *disp, uint16_t porta, uint8_t valor)
{
  if (porta == 0x0040) {
    estado_t1 *e = estado(disp);
    if (valor == 1) {
      e->instrucoes = e->instrucoes_lidas = e->milissegundos_lidos = 0;
      e->inicio = agora();
      e->medindo = true;
    } else if (valor == 2 && e->medindo) {
      e->instrucoes_lidas = e->instrucoes;
      e->milissegundos_lidos = (uint32_t)((agora() - e->inicio) * 1000.0);
    }
    return;
  }
  disp_escreve_byte_original(disp, porta, valor);
}

uint8_t disp_le_byte(disp_t *disp, uint16_t porta)
{
  estado_t1 *e = estado(disp);
  if (porta >= 0x0041 && porta <= 0x0044)
    return (uint8_t)(e->instrucoes_lidas >> (8 * (0x0044 - porta)));
  if (porta >= 0x0045 && porta <= 0x0048)
    return (uint8_t)(e->milissegundos_lidos >> (8 * (0x0048 - porta)));
  if (porta == 0x0049) return (uint8_t)((e->milissegundos_lidos / 1000) >> 8);
  if (porta == 0x004A) return (uint8_t)(e->milissegundos_lidos / 1000);
  if (porta == 0x0050) {
    e->aleatorio = sorteia();
    return (uint8_t)(e->aleatorio >> 8);
  }
  if (porta == 0x0051) return (uint8_t)e->aleatorio;
  return disp_le_byte_original(disp, porta);
}
