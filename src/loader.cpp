#include "limestone/loader.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

#include "limestone/csv.hpp"
#include "limestone/time.hpp"

namespace limestone {
namespace {

struct PendingStopTime {
    int trip = 0;
    int sequence = 0;
    int stop = 0;
    int arrival = 0;
    int departure = 0;
};

std::string join(const std::string& directory, const std::string& file) {
    if (!directory.empty() && directory.back() == '/') {
        return directory + file;
    }
    return directory + "/" + file;
}

double to_double(const std::string& text) {
    return text.empty() ? 0.0 : std::strtod(text.c_str(), nullptr);
}

int to_int(const std::string& text) {
    return text.empty() ? 0 : std::atoi(text.c_str());
}

void load_stops(const std::string& directory, Feed& feed) {
    CsvReader reader(join(directory, "stops.txt"));
    while (reader.next()) {
        const std::string& id = reader.get("stop_id");
        if (id.empty()) {
            continue;
        }

        const int index = feed.stop_ids.intern(id);
        if (index == static_cast<int>(feed.stops.size())) {
            feed.stops.push_back(Stop{reader.get("stop_name"), to_double(reader.get("stop_lat")),
                                      to_double(reader.get("stop_lon"))});
        }
    }
}

void load_routes(const std::string& directory, Feed& feed) {
    CsvReader reader(join(directory, "routes.txt"));
    while (reader.next()) {
        const std::string& id = reader.get("route_id");
        if (id.empty()) {
            continue;
        }

        const int index = feed.route_ids.intern(id);
        if (index == static_cast<int>(feed.routes.size())) {
            feed.routes.push_back(Route{reader.get("route_short_name"), reader.get("route_long_name")});
        }
    }
}

void load_trips(const std::string& directory, Feed& feed, LoadStats& stats) {
    CsvReader reader(join(directory, "trips.txt"));
    while (reader.next()) {
        const std::string& id = reader.get("trip_id");
        const int route = feed.route_ids.lookup(reader.get("route_id"));

        if (id.empty() || route == Interner::kMissing) {
            ++stats.skipped_trips;
            continue;
        }

        const int index = feed.trip_ids.intern(id);
        if (index != static_cast<int>(feed.trips.size())) {
            continue;
        }

        Trip trip;
        trip.route = route;
        trip.service = feed.service_ids.intern(reader.get("service_id"));
        trip.headsign = reader.get("trip_headsign");
        feed.trips.push_back(trip);
    }
}

// calendar_dates.txt carries exception_type 1 for an added date and 2 for a
// removed one. Kingston ships no calendar.txt, so every operating date is
// listed explicitly and only the additions matter.
void load_calendar_dates(const std::string& directory, Feed& feed) {
    feed.service_dates.assign(feed.service_ids.size(), {});

    CsvReader reader(join(directory, "calendar_dates.txt"));
    while (reader.next()) {
        const int service = feed.service_ids.lookup(reader.get("service_id"));
        if (service == Interner::kMissing) {
            continue;
        }

        const int exception = to_int(reader.get("exception_type"));
        if (exception != 1) {
            continue;
        }

        const int date = to_int(reader.get("date"));
        if (date > 0) {
            feed.service_dates[service].push_back(date);
        }
    }

    for (std::vector<int>& dates : feed.service_dates) {
        std::sort(dates.begin(), dates.end());
        dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
    }
}

void load_stop_times(const std::string& directory, Feed& feed, LoadStats& stats) {
    std::vector<PendingStopTime> pending;
    pending.reserve(200000);

    CsvReader reader(join(directory, "stop_times.txt"));
    while (reader.next()) {
        const int trip = feed.trip_ids.lookup(reader.get("trip_id"));
        const int stop = feed.stop_ids.lookup(reader.get("stop_id"));
        const std::optional<int> arrival = parse_gtfs_time(reader.get("arrival_time"));
        const std::optional<int> departure = parse_gtfs_time(reader.get("departure_time"));

        if (trip == Interner::kMissing || stop == Interner::kMissing || !arrival || !departure) {
            ++stats.skipped_stop_times;
            continue;
        }

        PendingStopTime entry;
        entry.trip = trip;
        entry.sequence = to_int(reader.get("stop_sequence"));
        entry.stop = stop;
        entry.arrival = *arrival;
        entry.departure = *departure;
        pending.push_back(entry);
    }

    // Grouping by trip and ordering by sequence lets each trip own a
    // contiguous slice, which is what the routing scan walks.
    std::sort(pending.begin(), pending.end(),
              [](const PendingStopTime& a, const PendingStopTime& b) {
                  if (a.trip != b.trip) {
                      return a.trip < b.trip;
                  }
                  return a.sequence < b.sequence;
              });

    feed.stop_times.reserve(pending.size());
    for (const PendingStopTime& entry : pending) {
        Trip& trip = feed.trips[entry.trip];
        if (trip.stop_time_count == 0) {
            trip.first_stop_time = static_cast<int>(feed.stop_times.size());
        }
        ++trip.stop_time_count;
        feed.stop_times.push_back(StopTime{entry.stop, entry.arrival, entry.departure});
    }
}

}

Feed load_feed(const std::string& directory, LoadStats* stats) {
    LoadStats local;
    LoadStats& counters = stats ? *stats : local;

    Feed feed;
    load_stops(directory, feed);
    load_routes(directory, feed);
    load_trips(directory, feed, counters);
    load_calendar_dates(directory, feed);
    load_stop_times(directory, feed, counters);

    if (feed.stops.empty() || feed.trips.empty()) {
        throw std::runtime_error("feed in " + directory + " has no stops or trips");
    }

    return feed;
}

}