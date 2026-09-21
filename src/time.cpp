#include "limestone/time.hpp"

#include <cstdio>

namespace limestone {

std::optional<int> parse_gtfs_time(const std::string& text) {
    std::size_t begin = text.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t end = text.find_last_not_of(" \t") + 1;

    int parts[3] = {0, 0, 0};
    int part_index = 0;
    int value = 0;
    bool saw_digit = false;

    for (std::size_t i = begin; i < end; ++i) {
        const char c = text[i];
        if (c == ':') {
            if (!saw_digit || part_index == 2) {
                return std::nullopt;
            }
            parts[part_index++] = value;
            value = 0;
            saw_digit = false;
            continue;
        }
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
        if (value > 999) {
            return std::nullopt;
        }
        saw_digit = true;
    }

    if (!saw_digit || part_index == 0) {
        return std::nullopt;
    }
    parts[part_index] = value;

    const int hours = parts[0];
    const int minutes = parts[1];
    const int seconds = part_index == 2 ? parts[2] : 0;

    if (minutes > 59 || seconds > 59) {
        return std::nullopt;
    }

    return hours * 3600 + minutes * 60 + seconds;
}

std::string format_hhmm(int seconds) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", seconds / 3600, (seconds % 3600) / 60);
    return buffer;
}

}