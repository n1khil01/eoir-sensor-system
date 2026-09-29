#include "ring_buffer.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

TEST(RingBufferTest, EmptyBufferHasZeroSize) {
    RingBuffer<int, 4> buffer;
    EXPECT_EQ(buffer.Size(), 0u);
}

TEST(RingBufferTest, SingleElementPushPopRoundTrips) {
    RingBuffer<int, 1> buffer;
    buffer.Push(42);
    EXPECT_EQ(buffer.Size(), 1u);
    EXPECT_EQ(buffer.Pop(), 42);
    EXPECT_EQ(buffer.Size(), 0u);
}

TEST(RingBufferTest, FullBufferHoldsExactlyCapacityItems) {
    RingBuffer<int, 3> buffer;
    buffer.Push(1);
    buffer.Push(2);
    buffer.Push(3);
    EXPECT_EQ(buffer.Size(), 3u);
}

// Pushes and pops well beyond Capacity so head_/tail_ wrap around the
// backing array multiple times, and checks strict FIFO order survives it.
TEST(RingBufferTest, WraparoundPreservesFifoOrder) {
    RingBuffer<int, 3> buffer;
    constexpr int kTotal = 20;  // more than 6x the capacity

    std::vector<int> received;
    received.reserve(kTotal);

    for (int i = 0; i < kTotal; ++i) {
        buffer.Push(i);
        received.push_back(buffer.Pop());
    }

    for (int i = 0; i < kTotal; ++i) {
        EXPECT_EQ(received[static_cast<size_t>(i)], i);
    }
}

// Interleaves pushes from more producers than the capacity holds at once
// with a lagging consumer, so head_/tail_ wrap while items are still
// in flight -- the case a purely sequential test can't exercise.
TEST(RingBufferTest, WraparoundUnderConcurrentProducerConsumer) {
    RingBuffer<int, 4> buffer;
    constexpr int kTotal = 200;

    std::thread producer([&] {
        for (int i = 0; i < kTotal; ++i) {
            buffer.Push(i);
        }
    });

    std::vector<int> received;
    received.reserve(kTotal);
    for (int i = 0; i < kTotal; ++i) {
        received.push_back(buffer.Pop());
    }
    producer.join();

    ASSERT_EQ(received.size(), static_cast<size_t>(kTotal));
    for (int i = 0; i < kTotal; ++i) {
        EXPECT_EQ(received[static_cast<size_t>(i)], i);
    }
}

// Push() is documented to increment overflow_count_ whenever the buffer is
// already full at the moment Push is called, even though it still blocks
// (rather than dropping the item) until space frees up.
TEST(RingBufferTest, OverflowCountIncrementsWhenPushingIntoFullBuffer) {
    RingBuffer<int, 2> buffer;
    buffer.Push(1);
    buffer.Push(2);
    EXPECT_EQ(buffer.overflow_count(), 0u);

    std::atomic<bool> push_returned{false};
    std::thread blocked_producer([&] {
        buffer.Push(3);  // buffer is full: increments overflow_count_, then blocks
        push_returned = true;
    });

    // Poll for the overflow to be recorded rather than sleeping a fixed
    // duration, so the test isn't flaky under CI scheduling noise.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (buffer.overflow_count() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    EXPECT_EQ(buffer.overflow_count(), 1u);
    EXPECT_FALSE(push_returned.load());  // still blocked: no space was freed yet

    EXPECT_EQ(buffer.Pop(), 1);  // frees a slot, unblocking the producer
    blocked_producer.join();
    EXPECT_TRUE(push_returned.load());
    EXPECT_EQ(buffer.Size(), 2u);
}

// Pop() blocks on an empty buffer rather than returning early or reading
// uninitialized storage; this exercises that path with a delayed producer.
TEST(RingBufferTest, PopBlocksUntilItemIsAvailable) {
    RingBuffer<int, 4> buffer;
    std::atomic<bool> pop_returned{false};
    int popped_value = -1;

    std::thread consumer([&] {
        popped_value = buffer.Pop();
        pop_returned = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(pop_returned.load());  // nothing pushed yet: consumer should still be blocked

    buffer.Push(99);
    consumer.join();
    EXPECT_TRUE(pop_returned.load());
    EXPECT_EQ(popped_value, 99);
}
