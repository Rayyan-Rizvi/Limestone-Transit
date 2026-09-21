#pragma once

#include <string>

#include "limestone/feed.hpp"

namespace limestone {

struct LoadStats {
    int skipped_stop_times = 0;
    int skipped_trips = 0;
};

// Reads a GTFS directory into a Feed. Throws if a required file is missing.
Feed load_feed(const std::string& directory, LoadStats* stats = nullptr);

}