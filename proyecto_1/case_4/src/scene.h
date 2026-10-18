// scene.h - Datos de la escena (esferas, luz, camara). Solo describe, no calcula.
// La interseccion y el sombreado van en raytracer.h/.cpp.
#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "vec3.h"

// Rayo: origin + t * dir, con t > 0. dir debe estar normalizado.
struct Ray {
    Vec3 origin;
    Vec3 dir;
};

// Parametros de Phong y de reflexion.
struct Material {
    Vec3   color;         // color base RGB en [0,1]
    double ambient;       // ka: luz ambiente
    double diffuse;       // kd: luz difusa (Lambert)
    double specular;      // ks: brillo especular
    double shininess;     // n: exponente especular (mayor = brillo mas pequeno)
    double reflectivity;  // 0..1: parte del color que viene del rayo reflejado
};

struct Sphere {
    Vec3     center;
    double   radius;
    Material mat;
};

// Luz puntual blanca: da sombras duras (sin penumbra).
struct Light {
    Vec3   position;
    double intensity;
};

// Camara pinhole mirando hacia +z, sin rotacion.
struct Camera {
    Vec3   position;
    double fov_degrees;  // campo de vision vertical

    // Rayo primario por el centro del pixel (px, py); (0,0) es arriba a la izquierda.
    Ray ray_for_pixel(int px, int py, int width, int height) const;
};

struct Scene {
    std::vector<Sphere> spheres;  // spheres[0] es siempre el piso
    Light               light;
    Camera              camera;
    Vec3                background;  // color si el rayo no golpea nada
};

// Escena determinista: mismo (num_spheres, seed) = misma escena en cualquier maquina.
// num_spheres NO cuenta el piso (el total real es num_spheres + 1).
Scene build_scene(int num_spheres, unsigned int seed);

#endif  // SCENE_H
