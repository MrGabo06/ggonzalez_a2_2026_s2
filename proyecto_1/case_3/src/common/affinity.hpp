#pragma once
#include <thread>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>

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

}  // namespace common

#else

namespace common {
inline bool pin_this_thread(int) { return false; }
inline unsigned hw_threads() { return std::thread::hardware_concurrency(); }
}  // namespace common

#endif
