#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>

// Corrected counterpart to racy_ring_buffer.hpp: identical structure and
// API, but `total_pushed_` is incremented inside the same critical section
// that already protects `count_`/`head_`/`tail_`, instead of after the
// lock is released. This is the fix, not a rewrite -- one line moved back
// inside the lock it should never have left.
template <typename T, size_t Capacity>
class FixedRingBuffer {
public:
    FixedRingBuffer() = default;

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
            ++total_pushed_;  // FIX: moved back inside the critical section
        }
        not_empty_.notify_one();
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

    size_t total_pushed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_pushed_;
    }

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
