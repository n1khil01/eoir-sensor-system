// Week 5 TCP detection-event test client.
//
// Connects to the DetectionEventServer, reads newline-delimited JSON
// events, and measures:
//   - detection-to-client delivery latency (client recv time minus the
//     server's steady_clock timestamp embedded in the event) -- valid
//     because steady_clock on a given host is a single shared clock
//     source across processes, so no NTP sync is needed for a loopback
//     or same-host test.
//   - sequence gaps, by checking the `seq` field is strictly consecutive.
//
// Usage: ./latency_client <host> <port> [num_events] [csv_path]
//
// Manual build (not wired into CMake, same convention as
// tools/week4_concurrency_repro):
//   g++ -std=c++17 -O2 -Iinclude tools/week5_tcp_client/latency_client.cpp \
//       -o build/latency_client
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "timing_stats.hpp"

namespace {

// The wire format is fixed and produced only by ToJsonLine() in
// detection_event.hpp, so a hand-rolled field extractor is fine here --
// no need to pull in a JSON library for one known shape.
std::optional<int64_t> ExtractIntField(const std::string& line,
                                        const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos += needle.size();
    return std::strtoll(line.c_str() + pos, nullptr, 10);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <host> <port> [num_events=1000] [csv_path]\n";
        return 1;
    }
    const char* host = argv[1];
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
    int num_events = argc > 3 ? std::atoi(argv[3]) : 1000;
    std::string csv_path =
        argc > 4 ? argv[4] : "benchmarks/week5_tcp_latency.csv";

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* resolved = nullptr;
    if (getaddrinfo(host, argv[2], &hints, &resolved) != 0) {
        std::cerr << "getaddrinfo failed for " << host << "\n";
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || connect(fd, resolved->ai_addr, resolved->ai_addrlen) < 0) {
        std::cerr << "connect() to " << host << ":" << port << " failed\n";
        freeaddrinfo(resolved);
        return 1;
    }
    freeaddrinfo(resolved);

    std::filesystem::create_directories(
        std::filesystem::path(csv_path).parent_path());
    std::ofstream csv(csv_path);
    csv << "event,seq,latency_us\n";

    std::vector<double> latency_us;
    latency_us.reserve(num_events);
    int64_t expected_seq = -1;
    int gap_count = 0;
    int received = 0;

    std::string buffer;
    char chunk[4096];
    while (received < num_events) {
        ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            std::cerr << "connection closed after " << received
                      << " events\n";
            break;
        }
        buffer.append(chunk, static_cast<size_t>(n));

        size_t newline;
        while ((newline = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);

            auto recv_ns = std::chrono::duration_cast<
                               std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now()
                                   .time_since_epoch())
                               .count();

            auto seq = ExtractIntField(line, "seq");
            auto ts_ns = ExtractIntField(line, "ts_ns");
            if (!seq || !ts_ns) continue;

            if (expected_seq >= 0 && *seq != expected_seq) {
                ++gap_count;
                std::cerr << "sequence gap: expected " << expected_seq
                          << " got " << *seq << "\n";
            }
            expected_seq = *seq + 1;

            double lat_us =
                static_cast<double>(recv_ns - *ts_ns) / 1000.0;
            latency_us.push_back(lat_us);
            csv << received << "," << *seq << "," << lat_us << "\n";
            ++received;
            if (received >= num_events) break;
        }
    }
    csv.close();
    close(fd);

    LatencyStats stats = ComputeLatencyStats(latency_us);
    std::cout << "received " << received << " events, " << gap_count
              << " sequence gaps\n";
    std::cout << "delivery latency (us): mean=" << stats.mean_us
              << " p95=" << stats.p95_us << " p99=" << stats.p99_us << "\n";
    std::cout << "raw per-event latencies written to " << csv_path << "\n";

    return 0;
}
