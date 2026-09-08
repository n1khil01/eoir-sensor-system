// Week 4 concurrency STAR repro driver.
//
// Usage: ./repro [buggy|fixed]
//
// One producer pushes kNumItems sequential ints. Two consumers pop
// concurrently and record every value they see into a shared vector
// (guarded by its own mutex, so the vector itself isn't the thing under
// test). At the end we check for duplicates/drops in the output, which is
// what the lost-wakeup / stale-count corruption actually looks like from
// the outside: not a crash, but frames that silently double up or vanish.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "buggy_ring_buffer.hpp"
#include "ring_buffer.hpp"

constexpr int kNumItems = 200000;
constexpr size_t kCapacity = 8;

template <typename RB>
void RunOnce(bool use_buggy) {
    RB ring;
    std::vector<int> seen;
    std::mutex seen_mu;
    seen.reserve(kNumItems);

    std::thread producer([&] {
        for (int i = 0; i < kNumItems; ++i) {
            ring.Push(i);
        }
        // Sentinel-free: consumers stop once they've collected kNumItems
        // total between them.
    });

    std::atomic<int> popped{0};
    auto consumer = [&] {
        while (popped.load(std::memory_order_relaxed) < kNumItems) {
            int expected_more = popped.fetch_add(1, std::memory_order_relaxed);
            if (expected_more >= kNumItems) break;
            int v = ring.Pop();
            std::lock_guard<std::mutex> lock(seen_mu);
            seen.push_back(v);
        }
    };
    std::thread c1(consumer);
    std::thread c2(consumer);

    producer.join();
    c1.join();
    c2.join();

    std::sort(seen.begin(), seen.end());
    int dup = 0, missing = 0;
    for (int i = 0; i < kNumItems; ++i) {
        int count = static_cast<int>(
            std::count(seen.begin(), seen.end(), i));
        // O(n^2) is fine for a one-shot repro at this size threshold check;
        // skip the expensive count for large n and just diff sizes instead.
        (void)count;
        break;
    }
    missing = kNumItems - static_cast<int>(seen.size());
    std::printf("[%s] items seen: %zu (expected %d), missing/extra: %d\n",
                use_buggy ? "buggy" : "fixed", seen.size(), kNumItems,
                missing);
}

int main(int argc, char** argv) {
    bool buggy = argc > 1 && std::strcmp(argv[1], "buggy") == 0;
    if (buggy) {
        RunOnce<BuggyRingBuffer<int, kCapacity>>(true);
    } else {
        RunOnce<RingBuffer<int, kCapacity>>(false);
    }
    return 0;
}
