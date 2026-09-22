#include "limestone/transfers.hpp"

#include <algorithm>
#include <cmath>

namespace limestone {
namespace {

constexpr double kEarthRadiusMetres = 6371000.0;
constexpr double kPi = 3.14159265358979323846;

double to_radians(double degrees) {
    return degrees * kPi / 180.0;
}

bool has_coordinates(const Stop& stop) {
    return stop.lat != 0.0 || stop.lon != 0.0;
}

int walking_seconds(double metres, const WalkingOptions& options) {
    const double walked = metres * options.detour_factor / options.metres_per_second;
    return std::max(options.minimum_seconds, static_cast<int>(std::ceil(walked)));
}

}

double haversine_metres(double lat1, double lon1, double lat2, double lon2) {
    const double dlat = to_radians(lat2 - lat1);
    const double dlon = to_radians(lon2 - lon1);
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(to_radians(lat1)) * std::cos(to_radians(lat2)) *
                         std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2.0 * kEarthRadiusMetres * std::asin(std::sqrt(a));
}

// Compares every pair of stops. For Kingston's 784 stops that is roughly
// 300,000 distance checks and takes milliseconds; a feed with tens of
// thousands of stops would want a spatial grid instead.
Transfers build_transfers(const Feed& feed, const WalkingOptions& options) {
    const int stop_count = static_cast<int>(feed.stops.size());
    std::vector<std::vector<Footpath>> adjacency(stop_count);

    for (int a = 0; a != stop_count; ++a) {
        const Stop& from = feed.stops[a];
        if (!has_coordinates(from)) {
            continue;
        }

        for (int b = a + 1; b != stop_count; ++b) {
            const Stop& to = feed.stops[b];
            if (!has_coordinates(to)) {
                continue;
            }

            const double metres = haversine_metres(from.lat, from.lon, to.lat, to.lon);
            if (metres > options.max_metres) {
                continue;
            }

            const int seconds = walking_seconds(metres, options);
            adjacency[a].push_back(Footpath{b, seconds});
            adjacency[b].push_back(Footpath{a, seconds});
        }
    }

    Transfers transfers;
    transfers.offsets.assign(stop_count + 1, 0);
    for (int s = 0; s != stop_count; ++s) {
        transfers.offsets[s + 1] = transfers.offsets[s] + static_cast<int>(adjacency[s].size());
        transfers.paths.insert(transfers.paths.end(), adjacency[s].begin(), adjacency[s].end());
    }

    return transfers;
}

std::vector<NearbyStop> stops_near(const Feed& feed, double lat, double lon, double radius_metres,
                                   const WalkingOptions& options) {
    std::vector<NearbyStop> nearby;

    for (std::size_t s = 0; s != feed.stops.size(); ++s) {
        const Stop& stop = feed.stops[s];
        if (!has_coordinates(stop)) {
            continue;
        }

        const double metres = haversine_metres(lat, lon, stop.lat, stop.lon);
        if (metres > radius_metres) {
            continue;
        }

        nearby.push_back(NearbyStop{static_cast<int>(s), walking_seconds(metres, options)});
    }

    return nearby;
}

}