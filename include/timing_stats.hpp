#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Mean/p95/p99 over a set of per-frame durations, in whatever unit the
// caller filled `samples_us` with (this project uses microseconds
// throughout so the three stages -- capture, process, total -- are
// directly comparable).
struct LatencyStats {
    double mean_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
};

inline LatencyStats ComputeLatencyStats(std::vector<double> samples_us) {
    LatencyStats stats;
    if (samples_us.empty()) {
        return stats;
    }

    double sum = 0.0;
    for (double v : samples_us) {
        sum += v;
    }
    stats.mean_us = sum / static_cast<double>(samples_us.size());

    std::sort(samples_us.begin(), samples_us.end());
    auto percentile_index = [&](double p) {
        size_t idx = static_cast<size_t>(p * static_cast<double>(samples_us.size() - 1));
        return std::min(idx, samples_us.size() - 1);
    };
    stats.p95_us = samples_us[percentile_index(0.95)];
    stats.p99_us = samples_us[percentile_index(0.99)];
    return stats;
}
