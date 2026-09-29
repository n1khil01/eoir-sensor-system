// Week 7 ThreadSanitizer STAR repro driver.
//
// Usage: ./race_driver [racy|fixed]
//
// Four producer threads share one ring buffer and each push kItemsPerProducer
// items; one consumer drains everything. `total_pushed()` is read once at
// the end purely to give the run a sanity-check line -- the actual defect
// under test is the *access pattern* during the run (concurrent, unlocked
// read-modify-write on total_pushed_ in the racy build), which is exactly
// what ThreadSanitizer instruments and reports on, independent of whether
// the final printed count happens to look right.
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "fixed_ring_buffer.hpp"
#include "racy_ring_buffer.hpp"

constexpr int kNumProducers = 4;
constexpr int kItemsPerProducer = 50000;
constexpr size_t kCapacity = 16;

template <typename RB>
void RunOnce(bool use_racy) {
    RB ring;
    constexpr int kTotal = kNumProducers * kItemsPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&] {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                ring.Push(i);
            }
        });
    }

    std::thread consumer([&] {
        for (int i = 0; i < kTotal; ++i) {
            ring.Pop();
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();

    std::printf("[%s] total_pushed()=%zu (expected %d)\n",
                use_racy ? "racy" : "fixed", ring.total_pushed(), kTotal);
}

int main(int argc, char** argv) {
    bool racy = argc > 1 && std::strcmp(argv[1], "racy") == 0;
    if (racy) {
        RunOnce<RacyRingBuffer<int, kCapacity>>(true);
    } else {
        RunOnce<FixedRingBuffer<int, kCapacity>>(false);
    }
    return 0;
}
