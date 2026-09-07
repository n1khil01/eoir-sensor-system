#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

// Reads the process's current resident set size from /proc/self/status.
// Used to show a flat RSS line over the Week 4 soak run as evidence the
// preallocated ring buffer isn't leaking under sustained concurrency.
// Returns 0 if the field can't be read (e.g. not running on Linux).
inline uint64_t CurrentRssKb() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::istringstream iss(line.substr(6));
            uint64_t kb = 0;
            iss >> kb;
            return kb;
        }
    }
    return 0;
}
