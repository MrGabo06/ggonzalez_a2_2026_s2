// Backend FPGA: ejecuta la convolución en el VPROC de la DE1-SoC por JTAG.
//
// Requiere la tarjeta programada con fpga/jtag/output_files/vproc_jtag.sof y Quartus
// (quartus_stp) instalado. La comunicación la hace fpga/jtag/vproc_jtag.tcl, que este backend
// arranca una sola vez y deja abierto mientras dura la medición (ver el protocolo en el .tcl).
//
// Por cada run():
//   1. Divide la imagen en franjas horizontales que caben en la memoria de datos (64 KB).
//      Cada franja lleva el borde replicado de R filas y columnas, como pide filter.h, y el
//      ancho se completa a múltiplo de 8 (los carriles del VPROC) repitiendo la última columna.
//   2. Por franja: escribe la entrada, arranca el núcleo, espera HALT y lee la salida.
//   3. Suma los ciclos que contó el hardware: kernel_ms() = ciclos / 50 MHz.
// El tiempo que mide el runner incluye las transferencias por JTAG (como las copias de la GPU);
// kernel_ms() es solo el cómputo en la FPGA, sin sesgo del host.
//
// Variables de entorno:
//   DLP_QUARTUS_STP  ruta de quartus_stp si no está en el PATH ni en ~/altera_lite o ~/intelFPGA_lite
//   DLP_FPGA_SIM=1   usa tclsh y la co-simulación de Icarus en lugar de la tarjeta (make fpga_jtag_sim)
//   DLP_FPGA_DMEM    bytes de memoria de datos a usar (por defecto 65536; menor para probar franjas)
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "backends/backend.h"

#ifndef DLP_ROOT
#define DLP_ROOT "."
#endif

namespace fs = std::filesystem;

namespace {

constexpr int kLanes = 8;
constexpr double kClockMHz = 50.0;
constexpr int kDefaultDmem = 65536;
constexpr int kRunTimeoutS = 60;

std::string Root() {
    const char* env = std::getenv("DLP_ROOT");
    return env ? env : DLP_ROOT;
}

bool SimMode() {
    const char* env = std::getenv("DLP_FPGA_SIM");
    return env && std::string(env) == "1";
}

bool IsExecutable(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec) && access(p.c_str(), X_OK) == 0;
}

// Busca quartus_stp: variable de entorno, PATH y las carpetas de instalación habituales.
std::string FindQuartusStp() {
    if (const char* env = std::getenv("DLP_QUARTUS_STP")) return IsExecutable(env) ? env : "";
    if (const char* path = std::getenv("PATH")) {
        std::stringstream ss(path);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            const fs::path stp = fs::path(dir) / "quartus_stp";
            if (!dir.empty() && IsExecutable(stp)) return stp.string();
        }
    }
    std::vector<fs::path> bases;
    if (const char* home = std::getenv("HOME")) {
        bases.push_back(fs::path(home) / "altera_lite");
        bases.push_back(fs::path(home) / "intelFPGA_lite");
        bases.push_back(fs::path(home) / "altera");
    }
    bases.push_back("/opt/altera_lite");
    bases.push_back("/opt/intelFPGA_lite");
    for (const auto& base : bases) {
        std::error_code ec;
        if (!fs::is_directory(base, ec)) continue;
        for (const auto& version : fs::directory_iterator(base, ec)) {
            const fs::path stp = version.path() / "quartus" / "bin" / "quartus_stp";
            if (IsExecutable(stp)) return stp.string();
        }
    }
    return "";
}

// Proceso hijo (quartus_stp o tclsh) con la entrada y la salida estándar conectadas por pipes.
class JtagServer {
public:
    // Los comandos y las respuestas van por dos FIFOs en dir y no por stdin/stdout, porque
    // quartus_stp retiene su salida estándar cuando no es una terminal. La salida de Quartus
    // (encabezado, avisos) queda en dir/quartus_stp.log.
    void Start(const std::string& exe, const std::vector<std::string>& argv, const fs::path& dir) {
        signal(SIGPIPE, SIG_IGN);  // si el hijo muere, fprintf falla en vez de matar el programa
        const fs::path cmd_fifo = dir / "cmd.fifo";
        const fs::path reply_fifo = dir / "reply.fifo";
        log_ = dir / "quartus_stp.log";
        for (const fs::path& f : {cmd_fifo, reply_fifo}) {
            std::error_code ec;
            fs::remove(f, ec);
            if (mkfifo(f.c_str(), 0600) != 0) throw std::runtime_error("no se pudo crear " + f.string());
        }
        setenv("VPROC_JTAG_CMD", cmd_fifo.c_str(), 1);
        setenv("VPROC_JTAG_REPLY", reply_fifo.c_str(), 1);

        pid_ = fork();
        if (pid_ < 0) throw std::runtime_error("fork() falló");
        if (pid_ == 0) {
            const int null_in = open("/dev/null", O_RDONLY);
            const int log = open(log_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (null_in >= 0) dup2(null_in, STDIN_FILENO);
            if (log >= 0) dup2(log, STDOUT_FILENO);
            std::vector<char*> args;
            for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
            args.push_back(nullptr);
            execv(exe.c_str(), args.data());
            std::fprintf(stderr, "No se pudo ejecutar %s: %s\n", exe.c_str(), std::strerror(errno));
            _exit(127);
        }

        // Abrir para escribir falla (ENXIO) hasta que el script abra el otro extremo; mientras
        // tanto se revisa que el proceso siga vivo para no quedarse esperando para siempre.
        int fd = -1;
        for (int ms = 0; fd < 0; ms += 50) {
            fd = open(cmd_fifo.c_str(), O_WRONLY | O_NONBLOCK);
            if (fd >= 0) break;
            int status = 0;
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                pid_ = -1;
                throw std::runtime_error("JTAG: " + exe + " terminó al arrancar. Revise " + log_.string());
            }
            if (ms > 120000) {
                Stop();
                throw std::runtime_error("JTAG: el script no abrió la comunicación en 2 min. Revise " + log_.string());
            }
            usleep(50000);
        }
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
        in_ = fdopen(fd, "w");
        out_ = std::fopen(reply_fifo.c_str(), "r");  // el script lo abre justo después
        if (!out_) {
            Stop();
            throw std::runtime_error("JTAG: no se pudo abrir " + reply_fifo.string());
        }
        const std::string ready = ReadReply();
        if (ready.rfind("READY", 0) != 0) {
            Stop();
            throw std::runtime_error("JTAG: " + ready);
        }
    }

    // Manda un comando y devuelve lo que sigue a "OK". Lanza si la respuesta es "ERR ...".
    std::string Call(const std::string& line) {
        if (!in_) throw std::runtime_error("JTAG: el servidor no está abierto");
        std::fprintf(in_, "%s\n", line.c_str());
        std::fflush(in_);
        const std::string reply = ReadReply();
        if (reply.rfind("OK", 0) != 0)
            throw std::runtime_error("JTAG (" + line.substr(0, line.find(' ')) + "): " + reply);
        return reply.size() > 3 ? reply.substr(3) : "";
    }

    void Stop() {
        if (in_) {
            std::fprintf(in_, "QUIT\n");
            std::fflush(in_);
            std::fclose(in_);
            in_ = nullptr;
        }
        if (out_) {
            std::fclose(out_);
            out_ = nullptr;
        }
        if (pid_ > 0) {
            int status = 0;
            waitpid(pid_, &status, 0);
            pid_ = -1;
        }
    }

    bool running() const { return in_ != nullptr; }
    ~JtagServer() { Stop(); }

private:
    // Lee líneas hasta una que empiece con "@@" (Quartus imprime su encabezado en la misma salida)
    std::string ReadReply() {
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), out_)) {
            std::string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            if (line.rfind("@@ ", 0) == 0) return line.substr(3);
        }
        return "ERR el proceso JTAG terminó sin responder (revise " + log_.string() + ")";
    }

    pid_t pid_ = -1;
    fs::path log_;
    FILE* in_ = nullptr;
    FILE* out_ = nullptr;
};

class FpgaBackend : public Backend {
public:
    std::string name() const override { return "fpga"; }

    bool available() const override { return SimMode() || !FindQuartusStp().empty(); }

    void init(const Image&, const Filter&) override { EnsureServer(); }

    void run(const Image& input, const Filter& filter, Image& output) override {
        EnsureServer();
        const int r = filter.size / 2;
        const int w = input.width;
        const int h = input.height;
        const int w8 = (w + kLanes - 1) / kLanes * kLanes;  // ancho de salida que calcula el VPROC
        const int wp = w8 + 2 * r;                          // ancho de la entrada con borde

        // Filas por franja: entrada (wp x (rows + 2r)) y salida (w8 x rows) en la memoria de datos
        const int dmem = DmemBytes();
        int rows = (dmem - wp * 2 * r) / (wp + w8);
        if (rows < 1) throw std::runtime_error("FPGA: la imagen es demasiado ancha para la memoria de datos");
        rows = std::min(rows, h);
        const int strips = (h + rows - 1) / rows;
        rows = (h + strips - 1) / strips;  // reparte las filas parejo entre las franjas
        const int in_base = 0;
        const int out_base = wp * (rows + 2 * r);

        LoadProgram(filter, w8, rows, wp, in_base, out_base);

        output = Image(w, h);
        long long total_cycles = 0;
        const fs::path in_hex = WorkDir() / "strip_in.hex";
        const fs::path out_hex = WorkDir() / "strip_out.hex";
        std::vector<uint8_t> strip(static_cast<size_t>(wp) * (rows + 2 * r));

        for (int s = 0; s < strips; ++s) {
            const int y0 = s * rows;
            // Franja con borde replicado: filas y columnas fuera de la imagen toman la más cercana
            for (int yy = 0; yy < rows + 2 * r; ++yy) {
                const int sy = std::clamp(y0 + yy - r, 0, h - 1);
                for (int xx = 0; xx < wp; ++xx) {
                    const int sx = std::clamp(xx - r, 0, w - 1);
                    strip[static_cast<size_t>(yy) * wp + xx] = input.at(sx, sy);
                }
            }
            WriteBytes(in_hex, strip);
            server_.Call("WRITE " + std::to_string(in_base) + " " + in_hex.string());

            std::istringstream result(server_.Call("RUN " + std::to_string(kRunTimeoutS)));
            long long cycles = 0;
            int error = 0;
            result >> cycles >> error;
            if (error) throw std::runtime_error("FPGA: el VPROC ejecutó una instrucción ilegal");
            total_cycles += cycles;

            const int n_out = w8 * rows;
            server_.Call("READ " + std::to_string(out_base) + " " + std::to_string(n_out) + " " +
                         out_hex.string());
            const std::vector<uint8_t> out = ReadBytes(out_hex, n_out);
            for (int yy = 0; yy < rows && y0 + yy < h; ++yy)
                for (int x = 0; x < w; ++x) output.at(x, y0 + yy) = out[static_cast<size_t>(yy) * w8 + x];
        }
        last_cycles_ = total_cycles;
    }

    double kernel_ms() const override {
        return last_cycles_ < 0 ? -1.0 : static_cast<double>(last_cycles_) / (kClockMHz * 1000.0);
    }

    void cleanup() override {
        server_.Stop();
        program_key_.clear();
    }

private:
    static int DmemBytes() {
        const char* env = std::getenv("DLP_FPGA_DMEM");
        const int v = env ? std::atoi(env) : kDefaultDmem;
        return (v > 0 && v <= kDefaultDmem) ? v : kDefaultDmem;
    }

    static fs::path WorkDir() {
        const fs::path dir = fs::path(Root()) / "build" / "fpga" / "jtag";
        fs::create_directories(dir);
        return dir;
    }

    void EnsureServer() {
        if (server_.running()) return;
        const std::string tcl = (fs::path(Root()) / "fpga" / "jtag" / "vproc_jtag.tcl").string();
        if (SimMode()) {
            server_.Start("/usr/bin/env", {"env", "tclsh", tcl, "server"}, WorkDir());
        } else {
            const std::string stp = FindQuartusStp();
            if (stp.empty()) throw std::runtime_error("FPGA: no se encontró quartus_stp (defina DLP_QUARTUS_STP)");
            server_.Start(stp, {stp, "-t", tcl, "server"}, WorkDir());
        }
        program_key_.clear();
    }

    // Ensambla el programa del filtro para este tamaño de franja y lo carga si cambió.
    void LoadProgram(const Filter& filter, int w8, int rows, int wp, int in_base, int out_base) {
        std::string source;
        if (filter.name == "gauss" && filter.size == 5) source = "gauss5.s";
        else if (filter.name == "sobel" && filter.size == 3) source = "sobel3.s";
        else throw std::runtime_error("FPGA: no hay programa del VPROC para el filtro " + filter.name);

        const std::string key = source + ":" + std::to_string(w8) + "x" + std::to_string(rows) + ":" +
                                std::to_string(wp) + ":" + std::to_string(out_base);
        if (key == program_key_) return;

        const fs::path root = Root();
        const fs::path prog = WorkDir() / "prog.hex";
        const std::string cmd = "python3 \"" + (root / "fpga" / "asm" / "vasm.py").string() + "\" \"" +
                                (root / "fpga" / "programs" / source).string() + "\" -o \"" + prog.string() +
                                "\" -D W=" + std::to_string(w8) + " -D H=" + std::to_string(rows) +
                                " -D WP=" + std::to_string(wp) + " -D IN=" + std::to_string(in_base) +
                                " -D OUT=" + std::to_string(out_base) + " > /dev/null";
        if (std::system(cmd.c_str()) != 0) throw std::runtime_error("FPGA: falló el ensamblador: " + cmd);
        server_.Call("PROG " + prog.string());
        program_key_ = key;
    }

    static void WriteBytes(const fs::path& path, const std::vector<uint8_t>& data) {
        std::ofstream f(path);
        if (!f) throw std::runtime_error("FPGA: no se puede escribir " + path.string());
        static const char* kHex = "0123456789ABCDEF";
        std::string text;
        text.reserve(data.size() * 3);
        for (uint8_t b : data) {
            text.push_back(kHex[b >> 4]);
            text.push_back(kHex[b & 15]);
            text.push_back('\n');
        }
        f << text;
    }

    static std::vector<uint8_t> ReadBytes(const fs::path& path, int n) {
        std::ifstream f(path);
        if (!f) throw std::runtime_error("FPGA: no se puede leer " + path.string());
        std::vector<uint8_t> out;
        out.reserve(n);
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            out.push_back(static_cast<uint8_t>(std::stoi(line, nullptr, 16)));
        }
        if (static_cast<int>(out.size()) != n)
            throw std::runtime_error("FPGA: se esperaban " + std::to_string(n) + " bytes y llegaron " +
                                     std::to_string(out.size()));
        return out;
    }

    JtagServer server_;
    std::string program_key_;
    long long last_cycles_ = -1;
};

}  // namespace

std::unique_ptr<Backend> make_fpga_backend() { return std::make_unique<FpgaBackend>(); }
