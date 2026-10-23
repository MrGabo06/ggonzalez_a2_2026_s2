// dlp_bench: CLI del framework. Carga o genera una imagen, ejecuta un backend
// N veces y reporta el tiempo de cada repetición.
//
// Uso:
//   dlp_bench --backend seq --size 1024 [--filter gauss|sobel] [--reps 5]
//             [--seed 42] [--input img.pgm] [--output out.pgm]
//             [--validate [--tol N]]
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "backends/backend.h"
#include "common/filter.h"
#include "common/image.h"
#include "common/validate.h"

namespace {

struct Options {
    std::string backend = "seq";
    std::string filter = "gauss";
    std::string input;                  // vacío = imagen sintética
    std::string output = "out.pgm";
    int size = 1024;
    int reps = 5;
    uint32_t seed = 42;
    bool validate = false;              // Compare against the baseline after timing
    int tolerance = 0;                  // Allowed per-pixel error for --validate
};

void usage() {
    std::cerr << "Uso: dlp_bench --backend <nombre> [--size N] [--filter gauss|sobel]\n"
                 "                 [--reps N] [--seed N] [--input img.pgm] [--output out.pgm]\n"
                 "                 [--validate [--tol N]]\n"
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
        else if (a == "--validate") o.validate = true;
        else if (a == "--tol") o.tolerance = std::stoi(next());
        else if (a == "--help" || a == "-h") { usage(); std::exit(0); }
        else { std::cerr << "Opción desconocida: " << a << "\n"; usage(); std::exit(1); }
    }
    return o;
}

// Compares the backend output against the sequential baseline, returns true if it passes
bool RunValidation(const Options& opt, const Image& input, const Filter& filter, const Image& output) {
    // Step 1: get the reference image from the sequential baseline
    auto baseline = create_backend("seq");
    Image reference;
    baseline->run(input, filter, reference);

    // Step 2: compare the backend output against it
    const ValidationResult result = Validate(reference, output, opt.tolerance);

    // Step 3: print the report
    std::cout << "validate," << opt.backend << "," << opt.filter << ","
              << input.width << "x" << input.height
              << ",max=" << result.max_abs_error << ",diff=" << result.differing_pixels
              << ",mse=" << result.mse << ",psnr=" << result.psnr
              << ",ok=" << result.ok << "\n";
    return result.ok;
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

        // Exit code 3 when validation fails, so scripts can detect it
        if (opt.validate && !RunValidation(opt, input, filter, output)) return 3;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}