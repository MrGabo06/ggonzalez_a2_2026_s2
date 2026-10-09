// render.cpp - Pintar un rectangulo de la imagen, y los bloques de filas de un hilo.
#include "render.h"
#include "raytracer.h"

#include <algorithm>  // std::min

void render_region(const Scene& scene, Image& img,
                   int x0, int y0, int x1, int y1, int max_depth) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            img.set_pixel(x, y, render_pixel(scene, x, y, img.width, img.height, max_depth));
        }
    }
}

void render_blocks(const Scene& scene, Image& img, int t, int n, int max_depth) {
    // Cantidad de bloques, redondeando hacia arriba (el ultimo puede ser mas corto).
    int nblocks = (img.height + ROW_BLOCK - 1) / ROW_BLOCK;
    // Al hilo t le tocan los bloques t, t+n, t+2n, ... (reparto ciclico).
    for (int b = t; b < nblocks; b += n) {
        int y0 = b * ROW_BLOCK;
        int y1 = std::min(y0 + ROW_BLOCK, img.height);
        render_region(scene, img, 0, y0, img.width, y1, max_depth);
    }
}
