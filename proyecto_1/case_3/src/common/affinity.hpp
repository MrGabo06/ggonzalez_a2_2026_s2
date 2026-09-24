#pragma once
#include <numeric>
#include <thread>
#include <vector>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>

#include <fstream>
#include <set>
#include <string>
#include <utility>

namespace common {

// Pinning is required so CMP maps one thread per physical core and SMT places two
// threads on each core's logical siblings.
inline bool pin_this_thread(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return sched_setaffinity(0, sizeof(set), &set) == 0;
}

inline bool pin_pthread(pthread_t thread, int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(thread, sizeof(set), &set) == 0;
}

inline unsigned hw_threads() { return std::thread::hardware_concurrency(); }

inline int read_topology_id(int cpu, const char* field) {
    std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(cpu) +
                    "/topology/" + field);
    int id = -1;
    f >> id;
    return f ? id : -1;
}

// Thread k is pinned to entry k: with one logical CPU per physical core listed
// first, T <= cores gives pure CMP and every thread beyond lands on a sibling.
inline std::vector<int> cpus_cores_first() {
    std::vector<int> primary, siblings;
    std::set<std::pair<int, int>> seen_cores;
    for (int cpu = 0; cpu < static_cast<int>(hw_threads()); ++cpu) {
        const int package = read_topology_id(cpu, "physical_package_id");
        const int core = read_topology_id(cpu, "core_id");
        const bool first_on_core =
            package < 0 || core < 0 || seen_cores.insert({package, core}).second;
        (first_on_core ? primary : siblings).push_back(cpu);
    }
    primary.insert(primary.end(), siblings.begin(), siblings.end());
    return primary;
}

}  // namespace common

#else

namespace common {
inline bool pin_this_thread(int) { return false; }
inline unsigned hw_threads() { return std::thread::hardware_concurrency(); }
inline std::vector<int> cpus_cores_first() {
    std::vector<int> cpus(hw_threads());
    std::iota(cpus.begin(), cpus.end(), 0);
    return cpus;
}
}  // namespace common

#endif
