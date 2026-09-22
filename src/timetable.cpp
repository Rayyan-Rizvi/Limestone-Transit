#include "limestone/timetable.hpp"

#include <algorithm>
#include <map>

namespace limestone {

Timetable build_timetable(const Feed& feed, int date) {
    Timetable timetable;
    timetable.date = date;
    timetable.stop_count = static_cast<int>(feed.stops.size());

    std::map<std::vector<int>, std::vector<int>> groups;
    for (std::size_t t = 0; t < feed.trips.size(); ++t) {
        const Trip& trip = feed.trips[t];
        if (trip.stop_time_count < 2 || !feed.service_runs_on(trip.service, date)) {
            continue;
        }

        std::vector<int> stops;
        stops.reserve(trip.stop_time_count);
        for (int i = 0; i < trip.stop_time_count; ++i) {
            stops.push_back(feed.stop_times[trip.first_stop_time + i].stop);
        }
        groups[stops].push_back(static_cast<int>(t));
    }

    for (auto& [stops, trips] : groups) {
        std::sort(trips.begin(), trips.end(), [&feed](int a, int b) {
            const int first_a = feed.stop_times[feed.trips[a].first_stop_time].departure;
            const int first_b = feed.stop_times[feed.trips[b].first_stop_time].departure;
            return first_a < first_b;
        });

        Pattern pattern;
        pattern.first_stop = static_cast<int>(timetable.pattern_stops.size());
        pattern.stop_count = static_cast<int>(stops.size());
        pattern.first_trip = static_cast<int>(timetable.pattern_trips.size());
        pattern.trip_count = static_cast<int>(trips.size());
        pattern.first_time = static_cast<int>(timetable.arrivals.size());
        pattern.route = feed.trips[trips.front()].route;

        timetable.pattern_stops.insert(timetable.pattern_stops.end(), stops.begin(), stops.end());

        for (const int t : trips) {
            timetable.pattern_trips.push_back(t);
            const Trip& trip = feed.trips[t];
            for (int i = 0; i < trip.stop_time_count; ++i) {
                const StopTime& call = feed.stop_times[trip.first_stop_time + i];
                timetable.arrivals.push_back(call.arrival);
                timetable.departures.push_back(call.departure);
            }
        }

        timetable.patterns.push_back(pattern);
    }

    // Build the stop -> (pattern, position) index in compressed form: one
    // offsets array plus one flat entries array, rather than a vector per stop.
    std::vector<int> counts(timetable.stop_count, 0);
    for (const Pattern& p : timetable.patterns) {
        for (int i = 0; i < p.stop_count; ++i) {
            ++counts[timetable.stop_at(p, i)];
        }
    }

    timetable.stop_pattern_offsets.assign(timetable.stop_count + 1, 0);
    for (int s = 0; s < timetable.stop_count; ++s) {
        timetable.stop_pattern_offsets[s + 1] = timetable.stop_pattern_offsets[s] + counts[s];
    }

    timetable.stop_patterns.resize(timetable.stop_pattern_offsets.back());
    std::vector<int> cursor(timetable.stop_pattern_offsets.begin(),
                            timetable.stop_pattern_offsets.end() - 1);

    for (std::size_t p = 0; p < timetable.patterns.size(); ++p) {
        const Pattern& pattern = timetable.patterns[p];
        for (int i = 0; i < pattern.stop_count; ++i) {
            const int stop = timetable.stop_at(pattern, i);
            timetable.stop_patterns[cursor[stop]++] = PatternStop{static_cast<int>(p), i};
        }
    }

    return timetable;
}

}