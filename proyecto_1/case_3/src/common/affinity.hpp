#pragma once
#include <cstddef>
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

inline int core_of(int cpu) { return read_topology_id(cpu, "core_id"); }

inline int current_cpu() { return sched_getcpu(); }

// Filling both hardware contexts of a core before the next one runs T threads on
// T/2 cores, which isolates what SMT adds over CMP at the same thread count.
inline std::vector<int> cpus_siblings_first() {
    const std::vector<int> cores_first = cpus_cores_first();
    std::vector<int> packed;
    std::set<int> taken;
    for (int cpu : cores_first) {
        if (taken.count(cpu)) continue;
        for (int other : cores_first) {
            const bool same_core =
                other == cpu ||
                (core_of(other) >= 0 && core_of(other) == core_of(cpu) &&
                 read_topology_id(other, "physical_package_id") ==
                     read_topology_id(cpu, "physical_package_id"));
            if (same_core && taken.insert(other).second) packed.push_back(other);
        }
    }
    return packed;
}

// 1 = SMT on, 0 = off (e.g. disabled in the BIOS), -1 = unknown.
inline int smt_active() {
    std::ifstream f("/sys/devices/system/cpu/smt/active");
    int active = -1;
    f >> active;
    return f ? active : -1;
}

// sysfs reports sizes such as "256K"; 0 means unknown.
inline std::size_t l2_cache_bytes() {
    std::ifstream f("/sys/devices/system/cpu/cpu0/cache/index2/size");
    std::size_t kib = 0;
    f >> kib;
    return f ? kib * 1024 : 0;
}

}  // namespace common

#else

namespace common {
inline bool pin_this_thread(int) { return false; }
inline unsigned hw_threads() { return std::thread::hardware_concurrency(); }
inline std::size_t l2_cache_bytes() { return 0; }
inline int core_of(int) { return -1; }
inline int current_cpu() { return -1; }
inline int smt_active() { return -1; }
inline std::vector<int> cpus_cores_first() {
    std::vector<int> cpus(hw_threads());
    std::iota(cpus.begin(), cpus.end(), 0);
    return cpus;
}
inline std::vector<int> cpus_siblings_first() { return cpus_cores_first(); }
}  // namespace common

#endif
