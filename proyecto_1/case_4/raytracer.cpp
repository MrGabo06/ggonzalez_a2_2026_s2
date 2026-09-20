// raytracer.cpp - Calculo del color de un rayo (Phong + sombras + reflexion).
#include "raytracer.h"

#include <cmath>    // std::sqrt, std::pow
#include <cstddef>  // std::size_t
#include <limits>   // std::numeric_limits

bool intersect_sphere(const Sphere& s, const Ray& ray, double t_max, double& t) {
    // Se resuelve |origin + t*dir - center|^2 = radius^2. Con dir unitario:
    //   t^2 + 2*b*t + c = 0   ->   t = -b -+ sqrt(b^2 - c)
    Vec3 oc = ray.origin - s.center;
    double b = dot(oc, ray.dir);
    double c = dot(oc, oc) - s.radius * s.radius;
    double disc = b * b - c;
    if (disc < 0.0) return false;  // el rayo no toca la esfera

    double root = std::sqrt(disc);
    double t0 = -b - root;  // punto de entrada
    if (t0 > RAY_EPSILON && t0 < t_max) { t = t0; return true; }
    double t1 = -b + root;  // punto de salida (rayo que empieza dentro)
    if (t1 > RAY_EPSILON && t1 < t_max) { t = t1; return true; }
    return false;
}

bool closest_hit(const Scene& scene, const Ray& ray, Hit& hit) {
    double t_best = std::numeric_limits<double>::infinity();
    int best = -1;
    for (std::size_t i = 0; i < scene.spheres.size(); ++i) {
        double t;
        // t_best como t_max: solo cuenta lo que este mas cerca que el mejor hasta ahora.
        if (intersect_sphere(scene.spheres[i], ray, t_best, t)) {
            t_best = t;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) return false;

    hit.t      = t_best;
    hit.sphere = best;
    hit.point  = ray.origin + ray.dir * t_best;
    hit.normal = normalize(hit.point - scene.spheres[best].center);
    return true;
}

bool in_shadow(const Scene& scene, const Vec3& point) {
    Vec3 to_light = scene.light.position - point;
    double dist = length(to_light);
    Ray shadow_ray;
    shadow_ray.origin = point;
    shadow_ray.dir    = to_light * (1.0 / dist);
    // Basta un objeto antes de llegar a la luz: no hace falta el mas cercano.
    for (std::size_t i = 0; i < scene.spheres.size(); ++i) {
        double t;
        if (intersect_sphere(scene.spheres[i], shadow_ray, dist, t)) return true;
    }
    return false;
}

Vec3 trace(const Scene& scene, const Ray& ray, int depth) {
    Hit hit;
    if (!closest_hit(scene, ray, hit)) return scene.background;

    const Material& m = scene.spheres[hit.sphere].mat;

    // Ambiente: siempre presente, incluso en sombra.
    Vec3 color = m.color * m.ambient;

    // Difuso y especular solo si la luz llega al punto.
    if (!in_shadow(scene, hit.point)) {
        Vec3 L = normalize(scene.light.position - hit.point);  // hacia la luz

        // Difuso (Lambert): proporcional a cos(angulo entre normal y luz).
        double diff = dot(hit.normal, L);
        if (diff < 0.0) diff = 0.0;
        color += m.color * (scene.light.intensity * m.diffuse * diff);

        // Especular (Phong): (R.V)^n, con R la luz reflejada y V hacia el ojo.
        Vec3 R = reflect(L * -1.0, hit.normal);
        Vec3 V = ray.dir * -1.0;
        double rv = dot(R, V);
        if (rv > 0.0) {
            color += Vec3(1.0, 1.0, 1.0) * (scene.light.intensity * m.specular * std::pow(rv, m.shininess));
        }
    }

    // Reflexion: rayo espejo, con limite de profundidad para que termine.
    if (depth > 0 && m.reflectivity > 0.0) {
        Ray reflected;
        reflected.origin = hit.point;
        reflected.dir    = reflect(ray.dir, hit.normal);
        Vec3 reflected_color = trace(scene, reflected, depth - 1);
        color = color * (1.0 - m.reflectivity) + reflected_color * m.reflectivity;
    }

    return color;
}

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

Vec3 render_pixel(const Scene& scene, int px, int py, int width, int height, int max_depth) {
    Ray ray = scene.camera.ray_for_pixel(px, py, width, height);
    Vec3 c = trace(scene, ray, max_depth);
    // La suma de luces puede pasar de 1; se recorta para que sea un color valido.
    return Vec3(clamp01(c.x), clamp01(c.y), clamp01(c.z));
}
