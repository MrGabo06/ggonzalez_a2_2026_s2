// render.cpp - Pintar un rectangulo de la imagen.
#include "render.h"
#include "raytracer.h"

void render_region(const Scene& scene, Image& img,
                   int x0, int y0, int x1, int y1, int max_depth) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            img.set_pixel(x, y, render_pixel(scene, x, y, img.width, img.height, max_depth));
        }
    }
}
