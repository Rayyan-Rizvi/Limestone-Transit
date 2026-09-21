#pragma once

#include <optional>
#include <string>

namespace limestone {

// GTFS times are counted from noon minus twelve hours of the service day, so
// a trip running past midnight reports hours of 24 or more.
std::optional<int> parse_gtfs_time(const std::string& text);

std::string format_hhmm(int seconds);

}