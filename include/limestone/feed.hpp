#pragma once

#include <string>
#include <vector>

#include "limestone/interner.hpp"

namespace limestone {

struct Stop {
    std::string name;
    double lat = 0.0;
    double lon = 0.0;
};

struct Route {
    std::string short_name;
    std::string long_name;
};

// Stop times for one trip occupy the half-open range
// [first_stop_time, first_stop_time + stop_time_count) in Feed::stop_times,
// so a trip's calls are contiguous in memory and scanned without indirection.
struct Trip {
    int route = 0;
    int service = 0;
    std::string headsign;
    int first_stop_time = 0;
    int stop_time_count = 0;
};

struct StopTime {
    int stop = 0;
    int arrival = 0;
    int departure = 0;
};

// A service day, stored as YYYYMMDD so dates compare and sort as integers.
struct ServiceDate {
    int value = 0;
};

class Feed {
public:
    std::vector<Stop> stops;
    std::vector<Route> routes;
    std::vector<Trip> trips;
    std::vector<StopTime> stop_times;

    Interner stop_ids;
    Interner route_ids;
    Interner trip_ids;
    Interner service_ids;

    // Operating dates per service id, sorted, for binary search at query time.
    std::vector<std::vector<int>> service_dates;

    bool service_runs_on(int service, int date) const;
    int find_stop_by_name(const std::string& query) const;
    std::vector<int> search_stops(const std::string& query, std::size_t limit) const;
};

}