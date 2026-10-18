// tests/test_scene.cpp - Pruebas de vec3.h y scene.h/.cpp (no es parte del ray tracer).
// Compilar y correr desde proyecto_1/case_4:
//   g++ -std=c++17 -Wall -Wextra -Isrc src/scene.cpp tests/test_scene.cpp -o /tmp/test_scene && /tmp/test_scene
#include <cstdio>
#include <cmath>
#include "scene.h"

static int fails = 0;
// Si la condicion es falsa imprime cual fue y cuenta la falla.
#define CHECK(c) do { if (!(c)) { std::printf("FALLA: %s\n", #c); fails++; } } while (0)

int main() {
    // ---------- vec3.h ----------
    Vec3 a(3, 0, 4);
    CHECK(std::fabs(length(a) - 5.0) < 1e-12);                 // triangulo 3-4-5
    CHECK(std::fabs(length(normalize(a)) - 1.0) < 1e-12);
    CHECK(std::fabs(dot(Vec3(1,2,3), Vec3(4,5,6)) - 32.0) < 1e-12);  // 4+10+18
    // Rayo (1,-1,0) rebotando en piso horizontal (normal (0,1,0)) sale en (1,1,0).
    Vec3 r = reflect(Vec3(1, -1, 0), Vec3(0, 1, 0));
    CHECK(std::fabs(r.x - 1) < 1e-12 && std::fabs(r.y - 1) < 1e-12 && std::fabs(r.z) < 1e-12);
    // Normalizar el vector nulo no debe dar NaN.
    Vec3 z = normalize(Vec3(0, 0, 0));
    CHECK(z.x == 0 && z.y == 0 && z.z == 0);

    // ---------- camara ----------
    Scene s = build_scene(9, 42);
    // En 101x101 el pixel (50,50) es el centro: el rayo va recto a (0,0,1).
    Ray c = s.camera.ray_for_pixel(50, 50, 101, 101);
    CHECK(std::fabs(c.dir.x) < 1e-12 && std::fabs(c.dir.y) < 1e-12 && std::fabs(c.dir.z - 1) < 1e-12);
    // Esquina superior izquierda: izquierda (x<0) y arriba (y>0).
    Ray tl = s.camera.ray_for_pixel(0, 0, 100, 100);
    CHECK(tl.dir.x < 0 && tl.dir.y > 0);
    // Esquina inferior derecha: derecha (x>0) y abajo (y<0).
    Ray br = s.camera.ray_for_pixel(99, 99, 100, 100);
    CHECK(br.dir.x > 0 && br.dir.y < 0);
    CHECK(std::fabs(length(tl.dir) - 1.0) < 1e-12);            // direccion normalizada

    // ---------- escena ----------
    CHECK(s.spheres.size() == 10);        // 9 pedidas + piso
    CHECK(s.spheres[0].radius == 1000.0); // spheres[0] es el piso
    // Misma semilla = misma escena; otra semilla = escena distinta.
    Scene s2 = build_scene(9, 42), s3 = build_scene(9, 43);
    bool same = true, diff = false;
    for (size_t i = 0; i < s.spheres.size(); ++i) {
        if (s.spheres[i].center.x != s2.spheres[i].center.x ||
            s.spheres[i].radius   != s2.spheres[i].radius) same = false;
        if (s.spheres[i].center.x != s3.spheres[i].center.x) diff = true;
    }
    CHECK(same);
    CHECK(diff);
    // Cada esfera apoya sobre el piso (y == radio) y no se solapa con otra.
    bool ok_floor = true, ok_overlap = true;
    for (size_t i = 1; i < s.spheres.size(); ++i) {
        if (std::fabs(s.spheres[i].center.y - s.spheres[i].radius) > 1e-12) ok_floor = false;
        for (size_t j = i + 1; j < s.spheres.size(); ++j) {
            double d = length(s.spheres[i].center - s.spheres[j].center);
            if (d < s.spheres[i].radius + s.spheres[j].radius) ok_overlap = false;
        }
    }
    CHECK(ok_floor);
    CHECK(ok_overlap);
    // Bordes: 0 esferas deja solo el piso; 200 tambien funciona.
    CHECK(build_scene(0, 1).spheres.size() == 1);
    CHECK(build_scene(200, 1).spheres.size() == 201);

    if (fails == 0) std::printf("RESULTADO: todas las pruebas OK\n");
    else            std::printf("RESULTADO: %d falla(s)\n", fails);
    return fails;
}
