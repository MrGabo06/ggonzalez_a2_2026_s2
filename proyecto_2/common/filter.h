// Filtros de convolución soportados por el framework.
#pragma once
#include <string>
#include <vector>

struct Filter {
    std::string name;
    int size = 0;                 // lado del filtro (3 o 5), siempre impar
    std::vector<float> weights;   // size * size coeficientes, fila por fila
};

Filter make_gaussian5();   // Gaussiano 5x5 normalizado (suma 1)
Filter make_sobel_x3();    // Sobel 3x3 en dirección x

// Devuelve el filtro por nombre ("gauss" o "sobel"). Lanza std::invalid_argument si no existe.
Filter filter_by_name(const std::string& name);