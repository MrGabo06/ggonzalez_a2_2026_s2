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

#endif  // RENDER_H
