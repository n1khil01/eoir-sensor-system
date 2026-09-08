#pragma once

#include <cstdint>
#include <sstream>
#include <string>

// One detection-event record streamed to TCP clients. `sequence` is a
// single server-wide counter (not per-client) so every connected client
// can independently detect a gap in its own stream by checking for
// non-consecutive values.
struct DetectionEvent {
    uint64_t sequence = 0;
    int64_t timestamp_ns = 0;  // steady_clock, taken at detection time
    int16_t min_value = 0;
    int16_t max_value = 0;
    double mean_value = 0.0;
    bool heat_signature_detected = false;
};

// Newline-delimited JSON, one object per line. The delimiter is what makes
// this a message stream on top of TCP's byte stream -- see the Week 5
// framing note in docs/engineering-log.md for why that matters.
inline std::string ToJsonLine(const DetectionEvent& event) {
    std::ostringstream out;
    out << "{\"seq\":" << event.sequence
        << ",\"ts_ns\":" << event.timestamp_ns
        << ",\"min\":" << event.min_value
        << ",\"max\":" << event.max_value
        << ",\"mean\":" << event.mean_value
        << ",\"detected\":" << (event.heat_signature_detected ? "true" : "false")
        << "}\n";
    return out.str();
}
