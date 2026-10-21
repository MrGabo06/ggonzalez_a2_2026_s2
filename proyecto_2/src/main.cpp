// dlp_bench: CLI del framework. Carga o genera una imagen, ejecuta un backend
// N veces y reporta el tiempo de cada repetición.
//
// Uso:
//   dlp_bench --backend seq --size 1024 [--filter gauss|sobel] [--reps 5]
//             [--seed 42] [--input img.pgm] [--output out.pgm]
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "backends/backend.h"
#include "common/filter.h"
#include "common/image.h"

namespace {

struct Options {
    std::string backend = "seq";
    std::string filter = "gauss";
    std::string input;                  // vacío = imagen sintética
    std::string output = "out.pgm";
    int size = 1024;
    int reps = 5;
    uint32_t seed = 42;
};

void usage() {
    std::cerr << "Uso: dlp_bench --backend <nombre> [--size N] [--filter gauss|sobel]\n"
                 "                 [--reps N] [--seed N] [--input img.pgm] [--output out.pgm]\n"
                 "Backends compilados:";
    for (const auto& n : available_backends()) std::cerr << " " << n;
    std::cerr << "\n";
}

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { usage(); std::exit(1); }
            return argv[++i];
        };
        if (a == "--backend") o.backend = next();
        else if (a == "--filter") o.filter = next();
        else if (a == "--input") o.input = next();
        else if (a == "--output") o.output = next();
        else if (a == "--size") o.size = std::stoi(next());
        else if (a == "--reps") o.reps = std::stoi(next());
        else if (a == "--seed") o.seed = static_cast<uint32_t>(std::stoul(next()));
        else if (a == "--help" || a == "-h") { usage(); std::exit(0); }
        else { std::cerr << "Opción desconocida: " << a << "\n"; usage(); std::exit(1); }
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse_args(argc, argv);

    try {
        auto backend = create_backend(opt.backend);
        if (!backend) {
            std::cerr << "Backend '" << opt.backend << "' no existe o no fue compilado\n";
            usage();
            return 1;
        }
        if (!backend->available()) {
            std::cerr << "Backend '" << opt.backend << "' no está disponible en esta máquina\n";
            return 2;
        }

        const Filter filter = filter_by_name(opt.filter);
        const Image input = opt.input.empty() ? generate_synthetic(opt.size, opt.size, opt.seed)
                                              : read_pgm(opt.input);
        Image output;

        backend->init(input, filter);
        for (int r = 0; r < opt.reps; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            backend->run(input, filter, output);
            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << backend->name() << "," << filter.name << "," << input.width << "x"
                      << input.height << ",rep=" << r << "," << ms << " ms\n";
        }
        backend->cleanup();

        write_pgm(opt.output, output);
        std::cout << "Salida escrita en " << opt.output << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}