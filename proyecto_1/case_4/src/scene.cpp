// scene.cpp - Camara y generacion de la escena.
#include "scene.h"

#include <cmath>    // std::tan, std::sqrt, std::ceil
#include <cstdint>  // std::uint32_t
#include <random>   // std::mt19937

// M_PI no es estandar de C++, por eso se define a mano.
static const double PI = 3.14159265358979323846;

// Camara pinhole: el plano de imagen esta a distancia 1 frente a la camara.
Ray Camera::ray_for_pixel(int px, int py, int width, int height) const {
    double aspect = static_cast<double>(width) / static_cast<double>(height);
    double half_h = std::tan(fov_degrees * 0.5 * PI / 180.0);  // semialto del plano
    double half_w = half_h * aspect;                            // semiancho

    // +0.5 apunta al centro del pixel; en Y se invierte porque la fila 0 esta arriba.
    double u = (2.0 * (px + 0.5) / width - 1.0) * half_w;
    double v = (1.0 - 2.0 * (py + 0.5) / height) * half_h;

    Ray r;
    r.origin = position;
    r.dir    = normalize(Vec3(u, v, 1.0));
    return r;
}

Scene build_scene(int num_spheres, unsigned int seed) {
    Scene scene;

    scene.background = Vec3(0.53, 0.75, 0.95);          // azul cielo
    scene.light.position  = Vec3(-10.0, 20.0, -10.0);
    scene.light.intensity = 1.0;
    scene.camera.position    = Vec3(0.0, 4.0, -10.0);
    scene.camera.fov_degrees = 50.0;

    // Piso: esfera enorme, su punto mas alto queda en y = 0.
    Sphere floor_sphere;
    floor_sphere.center = Vec3(0.0, -1000.0, 0.0);
    floor_sphere.radius = 1000.0;
    floor_sphere.mat.color        = Vec3(0.6, 0.6, 0.6);
    floor_sphere.mat.ambient      = 0.1;
    floor_sphere.mat.diffuse      = 0.9;
    floor_sphere.mat.specular     = 0.2;
    floor_sphere.mat.shininess    = 10.0;
    floor_sphere.mat.reflectivity = 0.3;
    scene.spheres.push_back(floor_sphere);

    // mt19937 es identico en todos los compiladores; uniform_real_distribution no,
    // por eso se convierte a mano el entero de 32 bits a un real en [0,1).
    // El azar solo coloca las esferas; el render no usa muestreo (no es Monte Carlo).
    std::mt19937 rng(seed);
    auto next01 = [&rng]() { return static_cast<double>(rng()) / 4294967296.0; };

    // Cuadricula de `cols` columnas para que no se solapen al variar N.
    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(num_spheres))));
    if (cols < 1) cols = 1;
    const double spacing = 2.5;  // distancia entre centros

    for (int i = 0; i < num_spheres; ++i) {
        int row = i / cols;
        int col = i % cols;

        // Cada next01() va en su propia sentencia: el orden de evaluacion de
        // argumentos no esta definido en C++ y la escena cambiaria entre compiladores.
        double radius = 0.5 + 0.5 * next01();       // [0.5, 1.0)
        double jitter_x = (next01() - 0.5) * 0.5;   // +-0.25
        double jitter_z = (next01() - 0.5) * 0.5;
        double cr = 0.2 + 0.8 * next01();           // color en [0.2, 1.0)
        double cg = 0.2 + 0.8 * next01();
        double cb = 0.2 + 0.8 * next01();
        double refl = 0.2 + 0.3 * next01();         // reflectividad en [0.2, 0.5)

        Sphere s;
        // y = radius: la esfera apoya sobre el piso.
        s.center = Vec3((col - (cols - 1) / 2.0) * spacing + jitter_x,
                        radius,
                        row * spacing + jitter_z);
        s.radius = radius;
        s.mat.color        = Vec3(cr, cg, cb);
        s.mat.ambient      = 0.1;
        s.mat.diffuse      = 0.9;
        s.mat.specular     = 0.5;
        s.mat.shininess    = 50.0;
        s.mat.reflectivity = refl;
        scene.spheres.push_back(s);
    }

    return scene;
}
