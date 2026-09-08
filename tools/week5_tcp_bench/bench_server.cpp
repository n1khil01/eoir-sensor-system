// Week 5 TCP layer bench: exercises DetectionEventServer directly with
// synthetic events, independent of the sensor. The sensor pipeline
// (RunThreaded in src/main.cpp) publishes into the exact same server
// class, so the delivery-latency and sequence-integrity numbers this
// produces characterize the networking layer itself -- the piece that
// does not depend on which machine has the MLX90640 wired up.
//
// Usage: ./bench_server <port> <num_events> [period_ms]
//
// Manual build (not wired into CMake, same convention as
// tools/week4_concurrency_repro):
//   g++ -std=c++17 -O2 -Iinclude tools/week5_tcp_bench/bench_server.cpp \
//       src/tcp_server.cpp -o build/bench_server
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "detection_event.hpp"
#include "tcp_server.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <port> <num_events> [period_ms=125]\n";
        return 1;
    }
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    int num_events = std::atoi(argv[2]);
    int period_ms = argc > 3 ? std::atoi(argv[3]) : 125;  // 8 Hz default

    DetectionEventServer server(port);
    server.Start();
    std::cout << "bench server listening on port " << port
              << ", waiting for a client...\n";

    // Give a client time to connect before the stream starts, mirroring
    // how a real deployment would have the client already attached.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    for (int i = 0; i < num_events; ++i) {
        DetectionEvent event;
        event.sequence = static_cast<uint64_t>(i);
        event.timestamp_ns = std::chrono::duration_cast<
                                  std::chrono::nanoseconds>(
                                  std::chrono::steady_clock::now()
                                      .time_since_epoch())
                                  .count();
        event.min_value = 100;
        event.max_value = 900;
        event.mean_value = 500.0;
        event.heat_signature_detected = (i % 50 == 0);
        server.PublishEvent(event);
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }

    // Let the broadcaster drain the last events before tearing down.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    server.Stop();
    std::cout << "bench server done, " << num_events << " events published\n";
    return 0;
}
