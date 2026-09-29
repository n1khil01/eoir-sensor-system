#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>

// Deliberately racy variant of include/ring_buffer.hpp for the Week 7
// ThreadSanitizer STAR repro.
//
// Unlike the Week 4 repro (tools/week4_concurrency_repro), which produced a
// hang from a *logic* defect while every shared-variable access stayed
// correctly mutex-protected -- so TSan had nothing to report -- this
// variant adds a genuine unsynchronized memory access: `total_pushed_` is
// incremented *after* the lock is released, on the mistaken assumption
// that "it's just a counter, one increment can't need the lock." With a
// single producer this never manifests. With multiple producer threads
// sharing one buffer it is a real, TSan-detectable data race: two threads
// can perform the non-atomic read-modify-write on `total_pushed_`
// concurrently, and increments can be lost.
template <typename T, size_t Capacity>
class RacyRingBuffer {
public:
    RacyRingBuffer() = default;

    void Push(T item) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (count_ == Capacity) {
                ++overflow_count_;
            }
            not_full_.wait(lock, [&] { return count_ < Capacity; });
            buffer_[head_] = std::move(item);
            head_ = (head_ + 1) % Capacity;
            ++count_;
        }
        not_empty_.notify_one();
        // BUG: read-modify-write on a plain (non-atomic) size_t with no
        // lock held, reachable concurrently by every producer thread.
        ++total_pushed_;
    }

    T Pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [&] { return count_ > 0; });
        T item = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) % Capacity;
        --count_;
        lock.unlock();
        not_full_.notify_one();
        return item;
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    // Not lock-protected by design -- see the bug note on Push().
    size_t total_pushed() const { return total_pushed_; }

private:
    std::array<T, Capacity> buffer_{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    uint64_t overflow_count_ = 0;
    size_t total_pushed_ = 0;
};
