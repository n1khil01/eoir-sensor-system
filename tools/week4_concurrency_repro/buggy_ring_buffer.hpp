#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>

// Deliberately buggy variant of include/ring_buffer.hpp for the Week 4
// concurrency STAR repro: Pop() uses `if` on the wait predicate instead of
// the predicate-loop form. With a single consumer this is very hard to
// trigger (needs a genuine OS-level spurious wakeup). With multiple
// consumers it reproduces reliably every run: two consumers can both be
// woken by one Push, one wins the race to reacquire the lock and drains the
// item, and the second proceeds past `if` on a now-false precondition
// because it never re-checks count_ after waking.
template <typename T, size_t Capacity>
class BuggyRingBuffer {
public:
    BuggyRingBuffer() = default;

    void Push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (count_ == Capacity) {
            ++overflow_count_;
        }
        not_full_.wait(lock, [&] { return count_ < Capacity; });
        buffer_[head_] = std::move(item);
        head_ = (head_ + 1) % Capacity;
        ++count_;
        lock.unlock();
        not_empty_.notify_one();
    }

    // BUG: `if` instead of the predicate-loop form. Between wait() waking
    // this thread and it reacquiring the mutex, another consumer can drain
    // the last item, leaving count_ == 0. This thread never re-checks and
    // reads/decrements from an empty buffer.
    T Pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (count_ == 0) {
            not_empty_.wait(lock);
        }
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
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    uint64_t overflow_count_ = 0;
};
