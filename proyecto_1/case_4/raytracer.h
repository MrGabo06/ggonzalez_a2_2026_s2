// raytracer.h - Interseccion rayo-esfera, Phong con sombras duras y reflexion acotada.
#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "scene.h"

// Distancia minima valida sobre un rayo. Evita que un rayo que sale de una
// superficie vuelva a golpearla por error numerico (shadow acne).
const double RAY_EPSILON = 1e-4;

// Resultado de la interseccion mas cercana.
struct Hit {
    double t;       // distancia a lo largo del rayo
    int    sphere;  // indice en scene.spheres
    Vec3   point;
    Vec3   normal;  // unitaria, hacia afuera de la esfera
};

// Interseccion rayo-esfera. Devuelve el t mas cercano en (RAY_EPSILON, t_max).
bool intersect_sphere(const Sphere& s, const Ray& ray, double t_max, double& t);

// Esfera mas cercana golpeada por el rayo.
bool closest_hit(const Scene& scene, const Ray& ray, Hit& hit);

// true si algo bloquea la luz entre point y la luz (sombra dura).
bool in_shadow(const Scene& scene, const Vec3& point);

// Color del rayo. depth = rebotes de reflexion que aun se permiten.
Vec3 trace(const Scene& scene, const Ray& ray, int depth);

// Color final del pixel (px, py), con cada canal limitado a [0,1].
Vec3 render_pixel(const Scene& scene, int px, int py, int width, int height, int max_depth);

#endif  // RAYTRACER_H
