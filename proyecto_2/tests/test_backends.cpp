// Prueba que todos los backends compilados den la misma imagen que el secuencial (seq).
// Se ejecuta con: make test_backends [SIMD=1] [AVX512=1] [CUDA=1]
//
// Compara con tolerancia 0 (resultado idéntico, píxel por píxel) usando los dos filtros
// y varios tamaños: algunos no son múltiplos de 8 ni de 16 y otros no son cuadrados,
// que es donde fallan los bordes y las colas de los backends paralelos.
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "backends/backend.h"
#include "common/filter.h"
#include "common/image.h"
#include "common/validate.h"

// Definido en backends/seq/seq_backend.cpp
std::unique_ptr<Backend> make_seq_backend();

namespace {

struct Size {
    int width;
    int height;
};

// Incluye imágenes más chicas que el filtro (1x1, 1xN), tamaños justo antes y después de
// múltiplos de 8 y 16, y rectangulares.
const std::vector<Size> kSizes = {
    {1, 1},   {1, 7},    {7, 1},    {2, 3},    {7, 7},    {8, 8},    {9, 9},
    {13, 13}, {15, 15},  {16, 16},  {17, 17},  {31, 33},  {37, 50},  {100, 3},
    {3, 100}, {100, 100}, {257, 129}, {1023, 517}, {1024, 1024},
};

const std::vector<uint32_t> kSeeds = {42, 7};

// Tamaños de bloque extra para cuda_tiled (se leen de DLP_CUDA_BLOCK al crear el backend)
const std::vector<std::string> kTiledBlocks = {"16x16", "8x8", "32x8", "4x32", "32x32"};

int cases = 0;
int failures = 0;

// Corre un backend dos veces sobre la misma imagen y compara con la referencia.
// La segunda corrida detecta estado que sobrevive de una corrida a la otra.
void CheckBackend(const std::string& label, const std::string& name, const Filter& filter,
                  const Image& input, const Image& reference) {
    ++cases;
    try {
        auto backend = create_backend(name);
        backend->init(input, filter);
        for (int pass = 1; pass <= 2; ++pass) {
            Image output;
            backend->run(input, filter, output);
            const ValidationResult result = Validate(reference, output, 0);
            if (!result.ok) {
                ++failures;
                std::cerr << "FALLA " << label << " " << filter.name << " " << input.width
                          << "x" << input.height << " corrida " << pass
                          << " max=" << result.max_abs_error
                          << " pixeles distintos=" << result.differing_pixels << "\n";
                break;
            }
        }
        backend->cleanup();
    } catch (const std::exception& e) {
        ++failures;
        std::cerr << "ERROR " << label << " " << filter.name << " " << input.width << "x"
                  << input.height << ": " << e.what() << "\n";
    }
}

}  // namespace

int main() {
    // Backends que se prueban: los compilados, menos seq (es la referencia)
    std::vector<std::string> names;
    for (const std::string& name : available_backends()) {
        if (name == "seq") continue;
        auto backend = create_backend(name);
        if (!backend || !backend->available()) {
            std::cout << "OMITIDO " << name << " (no disponible en esta maquina)\n";
            continue;
        }
        names.push_back(name);
    }
    if (names.empty()) {
        std::cerr << "No hay backends para probar\n";
        return 1;
    }

    auto seq = make_seq_backend();
    for (const Filter& filter : {make_gaussian5(), make_sobel_x3()}) {
        for (const Size& size : kSizes) {
            for (uint32_t seed : kSeeds) {
                const Image input = generate_synthetic(size.width, size.height, seed);
                Image reference;
                seq->run(input, filter, reference);

                for (const std::string& name : names) {
                    if (name == "cuda_tiled") {
                        for (const std::string& block : kTiledBlocks) {
                            setenv("DLP_CUDA_BLOCK", block.c_str(), 1);
                            CheckBackend(name + "[" + block + "]", name, filter, input, reference);
                        }
                        unsetenv("DLP_CUDA_BLOCK");
                    } else {
                        CheckBackend(name, name, filter, input, reference);
                    }
                }
            }
        }
    }

    std::cout << "Backends probados:";
    for (const std::string& name : names) std::cout << " " << name;
    std::cout << "\n" << (cases - failures) << "/" << cases << " casos identicos a seq\n";
    return failures == 0 ? 0 : 1;
}
