// dlp_bench: CLI del framework. Carga o genera una imagen, ejecuta un backend
// N veces y reporta el tiempo de cada repetición y un resumen estadístico.
//
// Uso:
//   dlp_bench --backend seq --size 1024 [--filter gauss|sobel] [--reps 5]
//             [--warmup 1] [--seed 42] [--input img.pgm] [--output out.pgm]
//             [--validate [--tol N]] [--csv results.csv]
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "backends/backend.h"
#include "common/filter.h"
#include "common/image.h"
#include "common/stats.h"
#include "common/validate.h"

namespace {

struct Options {
    std::string backend = "seq";
    std::string filter = "gauss";
    std::string input;                  // vacío = imagen sintética
    std::string output = "out.pgm";
    int size = 1024;
    int reps = 5;
    int warmup = 1;                     // Untimed runs before measuring
    uint32_t seed = 42;
    bool validate = false;              // Compare against the baseline after timing
    int tolerance = 0;                  // Allowed per-pixel error for --validate
    std::string csv;                    // Empty = do not write a CSV file
};

void usage() {
    std::cerr << "Uso: dlp_bench --backend <nombre> [--size N] [--filter gauss|sobel]\n"
                 "                 [--reps N] [--warmup N] [--seed N] [--input img.pgm] [--output out.pgm]\n"
                 "                 [--validate [--tol N]] [--csv results.csv]\n"
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
        else if (a == "--warmup") o.warmup = std::stoi(next());
        else if (a == "--seed") o.seed = static_cast<uint32_t>(std::stoul(next()));
        else if (a == "--validate") o.validate = true;
        else if (a == "--tol") o.tolerance = std::stoi(next());
        else if (a == "--csv") o.csv = next();
        else if (a == "--help" || a == "-h") { usage(); std::exit(0); }
        else { std::cerr << "Opción desconocida: " << a << "\n"; usage(); std::exit(1); }
    }
    return o;
}

// Compares the backend output against the sequential baseline and prints the report
ValidationResult RunValidation(const Options& opt, const Image& input, const Filter& filter, const Image& output) {
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
    return result;
}

// Prints one summary line with the statistics of the timed repetitions
void PrintSummary(const Options& opt, const Backend& backend, const Filter& filter,
                  const Image& input, const std::vector<double>& times) {
    std::cout << "summary," << backend.name() << "," << filter.name << ","
              << input.width << "x" << input.height
              << ",reps=" << opt.reps << ",warmup=" << opt.warmup
              << ",median=" << Median(times) << ",mean=" << Mean(times)
              << ",stddev=" << StdDev(times)
              << ",min=" << *std::min_element(times.begin(), times.end())
              << ",max=" << *std::max_element(times.begin(), times.end()) << " ms\n";
}

// Appends one row to the CSV file, the header goes first only if the file is new.
// Validation columns stay empty when --validate was not used
void AppendCsvRow(const Options& opt, const Backend& backend, const Filter& filter,
                  const Image& input, const std::vector<double>& times,
                  const std::optional<ValidationResult>& validation) {
    // Step 1: the file is new if it does not exist or is empty
    const bool is_new = !std::filesystem::exists(opt.csv) || std::filesystem::file_size(opt.csv) == 0;

    // Step 2: open in append mode, so earlier rows are kept
    std::ofstream file(opt.csv, std::ios::app);
    if (!file) throw std::runtime_error("cannot open CSV file: " + opt.csv);

    // Step 3: header only once
    if (is_new) {
        file << "backend,filter,width,height,reps,warmup,median_ms,mean_ms,stddev_ms,min_ms,max_ms,"
                "max_abs_error,psnr,ok\n";
    }

    // Step 4: one row for this run
    file << backend.name() << "," << filter.name << "," << input.width << "," << input.height
         << "," << opt.reps << "," << opt.warmup << "," << Median(times) << "," << Mean(times)
         << "," << StdDev(times) << "," << *std::min_element(times.begin(), times.end())
         << "," << *std::max_element(times.begin(), times.end()) << ",";
    if (validation) {
        file << validation->max_abs_error << "," << validation->psnr << "," << validation->ok;
    } else {
        file << ",,";
    }
    file << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse_args(argc, argv);

    // Step 1: at least one timed repetition is needed to have statistics
    if (opt.reps < 1 || opt.warmup < 0) {
        std::cerr << "--reps must be at least 1 and --warmup cannot be negative\n";
        return 1;
    }

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

        // Step 2: warm-up runs, not timed (caches, CPU frequency and memory settle)
        for (int w = 0; w < opt.warmup; ++w) backend->run(input, filter, output);

        // Step 3: timed repetitions, keep every time for the summary
        std::vector<double> times;
        for (int r = 0; r < opt.reps; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            backend->run(input, filter, output);
            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            times.push_back(ms);
            std::cout << backend->name() << "," << filter.name << "," << input.width << "x"
                      << input.height << ",rep=" << r << "," << ms << " ms\n";
        }
        backend->cleanup();

        // Step 4: summary of the timed repetitions
        PrintSummary(opt, *backend, filter, input, times);

        write_pgm(opt.output, output);
        std::cout << "Salida escrita en " << opt.output << "\n";

        // Step 5: validate against the baseline if asked
        std::optional<ValidationResult> validation;
        if (opt.validate) validation = RunValidation(opt, input, filter, output);

        // Step 6: save the row if asked (before any early exit, so failures are recorded too)
        if (!opt.csv.empty()) AppendCsvRow(opt, *backend, filter, input, times, validation);

        // Exit code 3 when validation fails, so scripts can detect it
        if (validation && !validation->ok) return 3;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}