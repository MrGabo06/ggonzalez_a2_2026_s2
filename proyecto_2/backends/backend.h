// Interfaz común que implementa cada backend (secuencial, SIMD, CUDA, FPGA).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "common/filter.h"
#include "common/image.h"

class Backend {
public:
    virtual ~Backend() = default;

    virtual std::string name() const = 0;

    // false si el hardware o la compilación no lo soportan (por ejemplo, FPGA sin tarjeta).
    virtual bool available() const { return true; }

    // Reserva recursos (memoria en GPU, conexión JTAG...). No se incluye en la medición.
    virtual void init(const Image& /*input*/, const Filter& /*filter*/) {}

    // Aplica el filtro a toda la imagen. Es lo único que mide el runner.
    virtual void run(const Image& input, const Filter& filter, Image& output) = 0;

    // Libera lo reservado en init().
    virtual void cleanup() {}
};

// Crea el backend por nombre; devuelve nullptr si el nombre no existe o no fue compilado.
std::unique_ptr<Backend> create_backend(const std::string& name);

// Nombres de los backends compilados en este binario.
std::vector<std::string> available_backends();