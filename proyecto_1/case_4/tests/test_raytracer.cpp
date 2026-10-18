// tests/test_raytracer.cpp - Pruebas de raytracer.h/.cpp con casos calculados a mano.
// Compilar y correr desde proyecto_1/case_4:
//   g++ -std=c++17 -Wall -Wextra -Isrc src/scene.cpp src/raytracer.cpp tests/test_raytracer.cpp -o /tmp/test_rt && /tmp/test_rt
#include <cstdio>
#include <cmath>
#include "raytracer.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALLA: %s\n", #c); fails++; } } while (0)

static bool near(const Vec3& a, const Vec3& b, double eps = 1e-9) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

static Sphere make_sphere(Vec3 c, double r, Vec3 color, double spec, double refl) {
    Sphere s;
    s.center = c; s.radius = r;
    s.mat.color = color; s.mat.ambient = 0.1; s.mat.diffuse = 0.9;
    s.mat.specular = spec; s.mat.shininess = 1.0; s.mat.reflectivity = refl;
    return s;
}

// Escena minima: camara en el origen mirando a +z, luz en (0,0,-10), fondo azul.
static Scene base_scene() {
    Scene s;
    s.background = Vec3(0.1, 0.2, 0.3);
    s.light.position = Vec3(0, 0, -10);
    s.light.intensity = 1.0;
    s.camera.position = Vec3(0, 0, 0);
    s.camera.fov_degrees = 50.0;
    return s;
}

int main() {
    Ray fwd; fwd.origin = Vec3(0, 0, 0); fwd.dir = Vec3(0, 0, 1);
    Vec3 red(1, 0, 0);

    // ---------- interseccion rayo-esfera ----------
    Sphere sp = make_sphere(Vec3(0, 0, 5), 1.0, red, 0, 0);
    double t = 0;
    CHECK(intersect_sphere(sp, fwd, 1e9, t) && std::fabs(t - 4.0) < 1e-12);  // entra en z=4
    Ray up; up.origin = Vec3(0, 0, 0); up.dir = Vec3(0, 1, 0);
    CHECK(!intersect_sphere(sp, up, 1e9, t));                                 // falla
    Sphere behind = make_sphere(Vec3(0, 0, -5), 1.0, red, 0, 0);
    CHECK(!intersect_sphere(behind, fwd, 1e9, t));                            // esfera detras
    Ray inside; inside.origin = Vec3(0, 0, 5); inside.dir = Vec3(0, 0, 1);
    CHECK(intersect_sphere(sp, inside, 1e9, t) && std::fabs(t - 1.0) < 1e-12); // sale en z=6
    CHECK(!intersect_sphere(sp, fwd, 3.0, t));                                // t_max corta antes
    Ray grazing; grazing.origin = Vec3(1, 0, 0); grazing.dir = Vec3(0, 0, 1);
    CHECK(intersect_sphere(sp, grazing, 1e9, t) && std::fabs(t - 5.0) < 1e-6); // tangente

    // ---------- sombreado ----------
    Scene sc = base_scene();
    sc.spheres.push_back(make_sphere(Vec3(0, 0, 5), 1.0, red, 0, 0));
    // Rayo que no golpea nada -> color de fondo.
    CHECK(near(trace(sc, up, 3), sc.background));
    // Luz de frente: cos = 1, color = rojo * (ambiente 0.1 + difuso 0.9) = (1,0,0).
    CHECK(near(trace(sc, fwd, 0), Vec3(1, 0, 0)));
    // Con especular ks=1, n=1: R == V, se suma (1,1,1) sin recortar -> (2,1,1).
    sc.spheres[0].mat.specular = 1.0;
    CHECK(near(trace(sc, fwd, 0), Vec3(2, 1, 1)));
    // render_pixel recorta a [0,1]: pixel central de 1x1 va recto a +z.
    CHECK(near(render_pixel(sc, 0, 0, 1, 1, 0), Vec3(1, 1, 1)));

    // ---------- sombras ----------
    Scene sh = base_scene();
    sh.spheres.push_back(make_sphere(Vec3(0, 0, 5), 1.0, red, 1.0, 0));
    sh.spheres.push_back(make_sphere(Vec3(0, 0, -5), 1.0, Vec3(0, 1, 0), 0, 0));  // bloquea la luz
    CHECK(in_shadow(sh, Vec3(0, 0, 4)));
    CHECK(near(trace(sh, fwd, 0), Vec3(0.1, 0, 0)));  // solo ambiente
    // Sin el bloqueador el mismo punto recibe luz.
    sh.spheres.pop_back();
    CHECK(!in_shadow(sh, Vec3(0, 0, 4)));

    // ---------- reflexion ----------
    Scene rf = base_scene();
    rf.spheres.push_back(make_sphere(Vec3(0, 0, 5), 1.0, Vec3(0.5, 0.5, 0.5), 0, 0.5));
    rf.spheres.push_back(make_sphere(Vec3(0, 0, -5), 1.0, Vec3(0, 1, 0), 0, 0));  // se ve en el reflejo
    Vec3 c0 = trace(rf, fwd, 0);  // sin reflexion
    Vec3 c1 = trace(rf, fwd, 1);  // un rebote
    Ray rr; rr.origin = Vec3(0, 0, 4); rr.dir = Vec3(0, 0, -1);
    Vec3 refl_color = trace(rf, rr, 0);
    CHECK(!near(c0, c1));                                       // la reflexion cambia el color
    CHECK(near(c1, c0 * 0.5 + refl_color * 0.5));               // mezcla = (1-k)*propio + k*reflejo
    rf.spheres[0].mat.reflectivity = 0.0;
    CHECK(near(trace(rf, fwd, 3), trace(rf, fwd, 0)));          // k=0 -> profundidad no importa

    // ---------- imagen completa: sin NaN y dentro de [0,1] ----------
    Scene full = build_scene(12, 42);
    bool ok = true;
    for (int y = 0; y < 48; ++y) {
        for (int x = 0; x < 64; ++x) {
            Vec3 c = render_pixel(full, x, y, 64, 48, 3);
            if (!(c.x >= 0 && c.x <= 1 && c.y >= 0 && c.y <= 1 && c.z >= 0 && c.z <= 1)) ok = false;  // NaN falla aqui
        }
    }
    CHECK(ok);

    if (fails == 0) std::printf("RESULTADO: todas las pruebas OK\n");
    else            std::printf("RESULTADO: %d falla(s)\n", fails);
    return fails;
}
