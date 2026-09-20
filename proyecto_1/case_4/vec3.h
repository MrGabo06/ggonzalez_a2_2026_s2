// vec3.h - Vector 3D para posiciones, direcciones y colores RGB.
// Todo inline en el header: son funciones diminutas que se llaman millones de veces.
#ifndef VEC3_H
#define VEC3_H

#include <cmath>  // std::sqrt

struct Vec3 {
    double x, y, z;  // double: mas precision en la interseccion rayo-esfera

    Vec3() : x(0.0), y(0.0), z(0.0) {}
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }

    // Por escalar: cambia la longitud (o la intensidad de un color).
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }

    // Componente a componente: luz * color del material.
    Vec3 operator*(const Vec3& o) const { return Vec3(x * o.x, y * o.y, z * o.z); }

    // Para acumular contribuciones de color (difuso, especular, reflejo).
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

// Permite escribir 2.0 * v ademas de v * 2.0.
inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

// Producto punto. Con vectores unitarios da cos(angulo): base de Lambert y Phong.
inline double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double length(const Vec3& v) { return std::sqrt(dot(v, v)); }

// Vector unitario (longitud 1). Si es nulo lo devuelve igual, evita dividir entre 0.
inline Vec3 normalize(const Vec3& v) {
    double len = length(v);
    return (len > 0.0) ? v * (1.0 / len) : v;
}

// Reflexion de i sobre una normal unitaria n: r = i - 2 (i.n) n.
inline Vec3 reflect(const Vec3& i, const Vec3& n) {
    return i - n * (2.0 * dot(i, n));
}

#endif  // VEC3_H
