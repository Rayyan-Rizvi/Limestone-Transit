#pragma once

#include <string>
#include <utility>
#include <vector>

#include "limestone/feed.hpp"

namespace limestone_test {

constexpr int kDate = 20261014;

inline int hm(int hours, int minutes) {
    return hours * 3600 + minutes * 60;
}

inline void add_trip(limestone::Feed& feed, const std::string& route,
                     const std::vector<std::pair<std::string, int>>& calls) {
    limestone::Trip trip;
    trip.route = feed.route_ids.lookup(route);
    trip.service = 0;
    trip.first_stop_time = static_cast<int>(feed.stop_times.size());
    trip.stop_time_count = static_cast<int>(calls.size());

    for (const auto& [stop_id, time] : calls) {
        feed.stop_times.push_back(limestone::StopTime{feed.stop_ids.lookup(stop_id), time, time});
    }

    feed.trip_ids.intern("T" + std::to_string(feed.trips.size()));
    feed.trips.push_back(trip);
}

// Four stops on one north-south street. B2 is 100 m from B, close enough to
// walk; every other pair is over a kilometre apart.
inline limestone::Feed walking_network() {
    limestone::Feed feed;
    feed.service_ids.intern("DAILY");
    feed.service_dates.push_back({kDate});

    for (const std::string route : {"X", "Y"}) {
        feed.route_ids.intern(route);
        feed.routes.push_back(limestone::Route{route, route});
    }

    const std::vector<std::pair<std::string, double>> stops = {
        {"A", 44.2000}, {"B", 44.2100}, {"B2", 44.2109}, {"D", 44.2300}};
    for (const auto& [id, lat] : stops) {
        feed.stop_ids.intern(id);
        feed.stops.push_back(limestone::Stop{id, lat, -76.5000});
    }

    add_trip(feed, "X", {{"A", hm(8, 0)}, {"B", hm(8, 10)}});
    add_trip(feed, "Y", {{"B2", hm(8, 15)}, {"D", hm(8, 30)}});
    return feed;
}

}