// render.h - Unidad de trabajo compartida por todos los motores: pintar un rectangulo.
#ifndef RENDER_H
#define RENDER_H

#include "image.h"
#include "scene.h"

// Pinta los pixeles [x0,x1) x [y0,y1) de img. Cada pixel depende solo de
// (scene, x, y), asi que los rectangulos (tiles) se pueden pintar en cualquier
// orden y en paralelo sin coordinarse. Ahi esta el paralelismo del problema.
void render_region(const Scene& scene, Image& img,
                   int x0, int y0, int x1, int y1, int max_depth);

// Filas por bloque en el reparto ciclico entre hilos (ver render_blocks).
const int ROW_BLOCK = 8;

// Pinta los bloques de filas que le tocan al hilo `t` de `n` hilos: el bloque b
// (filas [b*ROW_BLOCK, (b+1)*ROW_BLOCK)) lo pinta el hilo b % n. Los costos por
// fila no son parejos (el cielo es barato, el piso con reflexiones cuesta ~3
// veces mas); con franjas contiguas los hilos con filas baratas terminaban antes
// y esperaban. Al intercalar bloques, cada hilo recibe una mezcla de filas
// baratas y caras y todos terminan casi a la vez. Es el mismo reparto para todos
// los motores, asi la comparacion entre modelos es justa.
void render_blocks(const Scene& scene, Image& img, int t, int n, int max_depth);

#endif  // RENDER_H
