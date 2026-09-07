#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>

// Hand-built, fixed-capacity, thread-safe ring buffer for the Week 4
// producer-consumer pipeline. Deliberately not std::queue-backed: the goal
// is a real data-structure story (index arithmetic, wraparound, capacity
// management) rather than a container wrapped in a mutex.
//
// Locking discipline: the mutex only ever protects the index/count update
// and a std::move of one slot. Callers build the item to push (or consume
// the item popped) entirely outside the lock, so the critical section never
// includes a frame copy -- that is the Week 4 "narrow the lock" story.
template <typename T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() = default;

    // Blocks until there is space, moves `item` into the buffer, and wakes
    // one waiting consumer. Returns the time spent inside the critical
    // section (lock acquisition plus any wait for space), in microseconds,
    // so the caller can track producer blocking latency.
    //
    // `overflow_count()` increments whenever the buffer is already full at
    // the moment Push is called -- i.e. the producer is about to outrun the
    // consumer -- even though Push still blocks rather than dropping the
    // frame.
    double Push(T item) {
        auto start = std::chrono::steady_clock::now();
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
        return std::chrono::duration<double, std::micro>(
                   std::chrono::steady_clock::now() - start)
            .count();
    }

    // Blocks until an item is available and returns it, moved out of the
    // buffer. Uses the predicate form of wait() so a spurious wakeup can
    // never let a consumer read from an empty buffer.
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

    uint64_t overflow_count() const { return overflow_count_; }

private:
    std::array<T, Capacity> buffer_{};
    size_t head_ = 0;  // next write index
    size_t tail_ = 0;  // next read index
    size_t count_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    uint64_t overflow_count_ = 0;
};
