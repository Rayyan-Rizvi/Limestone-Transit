#pragma once

#include <vector>

#include "limestone/feed.hpp"

namespace limestone {

struct Footpath {
    int to = 0;
    int seconds = 0;
};

struct WalkingOptions {
    double max_metres = 300.0;
    double metres_per_second = 1.2;
    // Streets are not straight lines, so real walks run longer than the
    // great-circle distance between two stops.
    double detour_factor = 1.3;
    int minimum_seconds = 30;
};

// Footpaths in compressed form: the paths leaving stop s occupy
// paths[offsets[s]] up to but not including paths[offsets[s + 1]].
struct Transfers {
    std::vector<int> offsets;
    std::vector<Footpath> paths;

    std::size_t size() const { return paths.size(); }
};

struct NearbyStop {
    int stop = 0;
    int seconds = 0;
};

double haversine_metres(double lat1, double lon1, double lat2, double lon2);

Transfers build_transfers(const Feed& feed, const WalkingOptions& options = WalkingOptions{});

// Every stop within radius_metres of a point, with the time to walk there.
std::vector<NearbyStop> stops_near(const Feed& feed, double lat, double lon, double radius_metres,
                                   const WalkingOptions& options = WalkingOptions{});

}