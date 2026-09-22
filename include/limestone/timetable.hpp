#pragma once

#include <vector>

#include "limestone/feed.hpp"

namespace limestone {

// Trips that visit exactly the same stop sequence. GTFS route_id does not
// guarantee this, since one route can run variants that skip or short-turn.
struct Pattern {
    int first_stop = 0;
    int stop_count = 0;
    int first_trip = 0;
    int trip_count = 0;
    int first_time = 0;
    int route = 0;
};

struct PatternStop {
    int pattern = 0;
    int position = 0;
};

// The feed flattened for one service date. Times are laid out trip-major
// within each pattern, so scanning a trip reads consecutive memory.
struct Timetable {
    int date = 0;
    int stop_count = 0;

    std::vector<Pattern> patterns;
    std::vector<int> pattern_stops;
    std::vector<int> pattern_trips;
    std::vector<int> arrivals;
    std::vector<int> departures;

    std::vector<int> stop_pattern_offsets;
    std::vector<PatternStop> stop_patterns;

    int stop_at(const Pattern& p, int position) const {
        return pattern_stops[p.first_stop + position];
    }
    int arrival(const Pattern& p, int trip, int position) const {
        return arrivals[p.first_time + trip * p.stop_count + position];
    }
    int departure(const Pattern& p, int trip, int position) const {
        return departures[p.first_time + trip * p.stop_count + position];
    }
};

Timetable build_timetable(const Feed& feed, int date);

}