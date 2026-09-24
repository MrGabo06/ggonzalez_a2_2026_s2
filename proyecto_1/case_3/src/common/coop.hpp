#pragma once
#include <algorithm>
#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <utility>
#include <vector>

// Cooperative scheduling shared by the fine- and coarse-grained models: both run
// H logical threads on one core with the same engine and differ only in when a
// logical thread yields.
namespace coop {

class Task {
public:
    struct promise_type {
        Task get_return_object() { return Task{Handle::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };
    using Handle = std::coroutine_handle<promise_type>;

    explicit Task(Handle h) : h_(h) {}
    Task(Task&& other) noexcept : h_(std::exchange(other.h_, {})) {}
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task& operator=(Task&&) = delete;
    ~Task() {
        if (h_) h_.destroy();
    }

    bool done() const { return h_.done(); }
    void resume() { h_.resume(); }

private:
    Handle h_;
};

inline std::suspend_always yield() { return {}; }

// Only switches forced by a yield are counted: handing the core over because a
// thread finished its work is not a decision of the yield policy being measured.
class RoundRobin {
public:
    void run(std::vector<Task>& tasks, std::vector<int>* trace = nullptr) {
        std::size_t live = tasks.size();
        while (live > 0) {
            for (int k = 0; k < static_cast<int>(tasks.size()); ++k) {
                if (tasks[k].done()) continue;
                if (last_yielded_ && k != last_) ++switches_;
                last_ = k;
                if (trace) trace->push_back(k);
                tasks[k].resume();
                last_yielded_ = !tasks[k].done();
                if (!last_yielded_) --live;
            }
        }
    }

    long long switches() const { return switches_; }

private:
    long long switches_ = 0;
    int last_ = -1;
    bool last_yielded_ = false;
};

inline void print_trace(const char* model, int step, const std::vector<int>& order) {
    constexpr std::size_t shown = 32;
    std::fprintf(stderr, "[%s] step %d order:", model, step);
    for (std::size_t i = 0; i < std::min(order.size(), shown); ++i) {
        std::fprintf(stderr, " H%d", order[i]);
    }
    if (order.size() > shown) std::fprintf(stderr, " ... (%zu resumes)", order.size());
    std::fputc('\n', stderr);
}

}  // namespace coop
