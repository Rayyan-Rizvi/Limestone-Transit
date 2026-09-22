#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "limestone/journey.hpp"
#include "limestone/loader.hpp"
#include "limestone/map.hpp"
#include "limestone/raptor.hpp"
#include "limestone/sweep.hpp"
#include "limestone/time.hpp"
#include "limestone/timetable.hpp"
#include "limestone/transfers.hpp"
#include "limestone/version.hpp"

namespace {

using Clock = std::chrono::steady_clock;

long long milliseconds_since(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

long long microseconds_since(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
}

int print_usage() {
    std::cerr << "usage:\n"
              << "  limestone info FEED_DIR\n"
              << "  limestone stops FEED_DIR QUERY\n"
              << "  limestone route FEED_DIR FROM TO YYYYMMDD HH:MM\n"
              << "  limestone reach FEED_DIR LAT,LON YYYYMMDD HH:MM\n"
              << "  limestone sweep FEED_DIR LAT,LON YYYYMMDD OUT.csv [THREADS]\n"
              << "  limestone map FEED_DIR LAT,LON YYYYMMDD OUT.svg [HH:MM] [THREADS]\n"
              << "  limestone --version\n"
              << "\n"
              << "FROM and TO may be stop ids or stop names. Quote names with spaces.\n";
    return 2;
}

int run_info(const std::string& directory) {
    const Clock::time_point started = Clock::now();

    limestone::LoadStats stats;
    const limestone::Feed feed = limestone::load_feed(directory, &stats);
    const long long load_ms = milliseconds_since(started);

    int first_date = 0;
    int last_date = 0;
    for (const std::vector<int>& dates : feed.service_dates) {
        if (dates.empty()) {
            continue;
        }
        if (first_date == 0 || dates.front() < first_date) {
            first_date = dates.front();
        }
        last_date = std::max(last_date, dates.back());
    }

    const limestone::Transfers transfers = limestone::build_transfers(feed);

    std::cout << "feed:        " << directory << '\n'
              << "stops:       " << feed.stops.size() << '\n'
              << "routes:      " << feed.routes.size() << '\n'
              << "trips:       " << feed.trips.size() << '\n'
              << "stop times:  " << feed.stop_times.size() << '\n'
              << "services:    " << feed.service_ids.size() << '\n'
              << "footpaths:   " << transfers.size() << '\n'
              << "dates:       " << first_date << " to " << last_date << '\n'
              << "load time:   " << load_ms << " ms\n";

    if (stats.skipped_stop_times > 0 || stats.skipped_trips > 0) {
        std::cout << "skipped:     " << stats.skipped_trips << " trips, "
                  << stats.skipped_stop_times << " stop times\n";
    }

    return 0;
}

int run_stops(const std::string& directory, const std::string& query) {
    const limestone::Feed feed = limestone::load_feed(directory);
    const std::vector<int> matches = feed.search_stops(query, 15);

    if (matches.empty()) {
        std::cout << "no stops matching \"" << query << "\"\n";
        return 1;
    }

    for (const int index : matches) {
        const limestone::Stop& stop = feed.stops[index];
        std::cout << feed.stop_ids.name(index) << "  " << stop.name << "  (" << stop.lat << ", "
                  << stop.lon << ")\n";
    }

    return 0;
}

// Accepts either a stop id or a stop name. Ids win, so a stop whose name
// happens to look like another stop's id is still reachable by id.
int resolve_stop(const limestone::Feed& feed, const std::string& text) {
    const int by_id = feed.stop_ids.lookup(text);
    if (by_id != limestone::Interner::kMissing) {
        return by_id;
    }

    const int by_name = feed.find_stop_by_name(text);
    if (by_name != -1) {
        return by_name;
    }

    throw std::runtime_error("no stop matches \"" + text + "\" (try the stops command)");
}

std::string describe_trips(int trips) {
    if (trips == 0) {
        return "walk only";
    }
    return std::to_string(trips) + (trips == 1 ? " bus" : " buses");
}

void print_journey(const limestone::Feed& feed, const limestone::Journey& journey) {
    std::cout << "  " << describe_trips(journey.trips) << ", arrive "
              << limestone::format_hhmm(journey.arrival) << '\n';

    for (const limestone::Leg& leg : journey.legs) {
        const std::string& from = feed.stops[leg.from_stop].name;
        const std::string& to = feed.stops[leg.to_stop].name;

        if (leg.kind == limestone::LegKind::Walk) {
            const int minutes = std::max(1, (leg.arrive - leg.depart + 59) / 60);
            std::cout << "    " << limestone::format_hhmm(leg.depart) << "  walk " << minutes
                      << " min to " << to << " (" << feed.stop_ids.name(leg.to_stop) << ")\n";
            continue;
        }

        const limestone::Route& route = feed.routes[leg.route];
        const std::string& route_name = route.short_name.empty() ? route.long_name : route.short_name;
        const std::string& headsign = feed.trips[leg.trip].headsign;

        std::cout << "    " << limestone::format_hhmm(leg.depart) << "  board route " << route_name;
        if (!headsign.empty()) {
            std::cout << " (" << headsign << ")";
        }
        std::cout << " at " << from << " (" << feed.stop_ids.name(leg.from_stop) << ")\n";

        std::cout << "    " << limestone::format_hhmm(leg.arrive) << "  get off at " << to
                  << " (" << feed.stop_ids.name(leg.to_stop) << ")\n";
    }
}

int run_route(const std::string& directory, const std::string& from, const std::string& to,
              const std::string& date_text, const std::string& time_text) {
    if (date_text.size() != 8) {
        throw std::runtime_error("date must be YYYYMMDD, got " + date_text);
    }
    const int date = std::atoi(date_text.c_str());

    const std::optional<int> departure = limestone::parse_gtfs_time(time_text);
    if (!departure) {
        throw std::runtime_error("time must be HH:MM, got " + time_text);
    }

    const limestone::Feed feed = limestone::load_feed(directory);

    limestone::Query query;
    query.source = resolve_stop(feed, from);
    query.target = resolve_stop(feed, to);
    query.departure = *departure;

    Clock::time_point started = Clock::now();
    const limestone::Timetable timetable = limestone::build_timetable(feed, date);
    const long long build_ms = milliseconds_since(started);

    if (timetable.patterns.empty()) {
        std::cout << "no service runs on " << date << " (check the date range with info)\n";
        return 1;
    }

    const limestone::Transfers transfers = limestone::build_transfers(feed);

    started = Clock::now();
    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
    const long long query_us = microseconds_since(started);

    std::cout << "from:      " << feed.stop_ids.name(query.source) << "  "
              << feed.stops[query.source].name << '\n'
              << "to:        " << feed.stop_ids.name(query.target) << "  "
              << feed.stops[query.target].name << '\n'
              << "departing: " << date << " at " << limestone::format_hhmm(query.departure) << '\n'
              << "patterns:  " << timetable.patterns.size() << " (built in " << build_ms << " ms)\n"
              << "query:     " << query_us << " microseconds\n";

    if (result.options.empty()) {
        std::cout << "\n  no journey found\n";
        return 1;
    }

    for (const limestone::Option& option : result.options) {
        const limestone::Journey journey =
            limestone::reconstruct_journey(timetable, result, query, option.trips);
        std::cout << '\n';
        print_journey(feed, journey);
    }

    return 0;
}

}

bool parse_point(const std::string& text, double& lat, double& lon) {
    const std::size_t comma = text.find(',');
    if (comma == std::string::npos) {
        return false;
    }

    char* end = nullptr;
    lat = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + comma) {
        return false;
    }

    lon = std::strtod(text.c_str() + comma + 1, &end);
    return *end == '\0';
}

int run_reach(const std::string& directory, const std::string& point,
              const std::string& date_text, const std::string& time_text) {
    double lat = 0.0;
    double lon = 0.0;
    if (!parse_point(point, lat, lon)) {
        throw std::runtime_error("point must be LAT,LON with no space, got " + point);
    }
    if (date_text.size() != 8) {
        throw std::runtime_error("date must be YYYYMMDD, got " + date_text);
    }
    const int date = std::atoi(date_text.c_str());

    const std::optional<int> departure = limestone::parse_gtfs_time(time_text);
    if (!departure) {
        throw std::runtime_error("time must be HH:MM, got " + time_text);
    }

    const limestone::Feed feed = limestone::load_feed(directory);

    // Starting from every stop within walking range of the point, rather than
    // the single nearest one, avoids biasing results toward one side of it.
    constexpr double kOriginRadiusMetres = 500.0;
    const std::vector<limestone::NearbyStop> nearby =
        limestone::stops_near(feed, lat, lon, kOriginRadiusMetres);
    if (nearby.empty()) {
        throw std::runtime_error("no stops within 500 m of " + point);
    }

    limestone::Query query;
    query.target = limestone::kNoTarget;
    query.departure = *departure;
    for (const limestone::NearbyStop& stop : nearby) {
        query.origins.push_back(limestone::Origin{stop.stop, *departure + stop.seconds});
    }

    const limestone::Timetable timetable = limestone::build_timetable(feed, date);
    if (timetable.patterns.empty()) {
        std::cout << "no service runs on " << date << " (check the date range with info)\n";
        return 1;
    }
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    const Clock::time_point started = Clock::now();
    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
    const long long query_us = microseconds_since(started);

    const int thresholds[] = {15, 30, 45, 60};
    int within[] = {0, 0, 0, 0};
    int unreachable = 0;

    for (const int arrival : result.best) {
        if (arrival == limestone::kUnreachable) {
            ++unreachable;
            continue;
        }
        const int minutes = (arrival - query.departure) / 60;
        for (int i = 0; i != 4; ++i) {
            if (minutes <= thresholds[i]) {
                ++within[i];
            }
        }
    }

    const int total = static_cast<int>(feed.stops.size());
    std::cout << "origin:    " << point << " (" << nearby.size() << " stops within 500 m)\n"
              << "departing: " << date << " at " << limestone::format_hhmm(query.departure) << '\n'
              << "query:     " << query_us << " microseconds\n\n";

    for (int i = 0; i != 4; ++i) {
        const int percent = (within[i] * 100 + total / 2) / total;
        std::cout << "  within " << thresholds[i] << " min:  " << within[i] << " of " << total
                  << " stops (" << percent << "%)\n";
    }
    std::cout << "  unreachable:    " << unreachable << " stops\n";

    return 0;
}

int run_sweep_command(const std::string& directory, const std::string& point,
                      const std::string& date_text, const std::string& output_path, int threads) {
    double lat = 0.0;
    double lon = 0.0;
    if (!parse_point(point, lat, lon)) {
        throw std::runtime_error("point must be LAT,LON with no space, got " + point);
    }
    if (date_text.size() != 8) {
        throw std::runtime_error("date must be YYYYMMDD, got " + date_text);
    }
    const int date = std::atoi(date_text.c_str());

    const limestone::Feed feed = limestone::load_feed(directory);

    constexpr double kOriginRadiusMetres = 500.0;
    const std::vector<limestone::NearbyStop> nearby =
        limestone::stops_near(feed, lat, lon, kOriginRadiusMetres);
    if (nearby.empty()) {
        throw std::runtime_error("no stops within 500 m of " + point);
    }

    std::vector<limestone::Origin> origins;
    for (const limestone::NearbyStop& stop : nearby) {
        origins.push_back(limestone::Origin{stop.stop, stop.seconds});
    }

    const limestone::Timetable timetable = limestone::build_timetable(feed, date);
    if (timetable.patterns.empty()) {
        std::cout << "no service runs on " << date << " (check the date range with info)\n";
        return 1;
    }
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    limestone::SweepOptions options;
    options.threads = threads;

    const Clock::time_point started = Clock::now();
    const limestone::SweepResult result =
        limestone::run_sweep(timetable, transfers, origins, options);
    const long long elapsed_ms = milliseconds_since(started);

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("cannot write " + output_path);
    }
    out << "stop_id,stop_name,lat,lon,reached,samples,median_minutes,best_minutes\n";

    int always = 0;
    int never = 0;
    for (std::size_t s = 0; s != result.stops.size(); ++s) {
        const limestone::StopAccess& access = result.stops[s];
        const limestone::Stop& stop = feed.stops[s];

        if (access.reached == access.samples) {
            ++always;
        }
        if (access.reached == 0) {
            ++never;
        }

        out << feed.stop_ids.name(static_cast<int>(s)) << ",\"" << stop.name << "\"," << stop.lat
            << ',' << stop.lon << ',' << access.reached << ',' << access.samples << ',';
        if (access.median_seconds < 0) {
            out << ",";
        } else {
            out << (access.median_seconds + 30) / 60 << ',' << (access.best_seconds + 30) / 60;
        }
        out << '\n';
    }

    const long long queries = static_cast<long long>(result.departures);
    std::cout << "origin:      " << point << " (" << nearby.size() << " stops within 500 m)\n"
              << "date:        " << date << '\n'
              << "departures:  " << queries << '\n'
              << "threads:     " << threads << '\n'
              << "elapsed:     " << elapsed_ms << " ms\n"
              << "per query:   " << (elapsed_ms * 1000.0 / static_cast<double>(queries))
              << " microseconds\n"
              << "always:      " << always << " stops reachable from every departure\n"
              << "never:       " << never << " stops never reachable\n"
              << "written to:  " << output_path << '\n';

    return 0;
}

int run_map(const std::string& directory, const std::string& point, const std::string& date_text,
            const std::string& output_path, const std::string& time_text, int threads) {
    double lat = 0.0;
    double lon = 0.0;
    if (!parse_point(point, lat, lon)) {
        throw std::runtime_error("point must be LAT,LON with no space, got " + point);
    }
    if (date_text.size() != 8) {
        throw std::runtime_error("date must be YYYYMMDD, got " + date_text);
    }
    const int date = std::atoi(date_text.c_str());

    const limestone::Feed feed = limestone::load_feed(directory);

    constexpr double kOriginRadiusMetres = 500.0;
    const std::vector<limestone::NearbyStop> nearby =
        limestone::stops_near(feed, lat, lon, kOriginRadiusMetres);
    if (nearby.empty()) {
        throw std::runtime_error("no stops within 500 m of " + point);
    }

    const limestone::Timetable timetable = limestone::build_timetable(feed, date);
    if (timetable.patterns.empty()) {
        std::cout << "no service runs on " << date << " (check the date range with info)\n";
        return 1;
    }
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    std::vector<int> minutes(feed.stops.size(), -1);
    std::string subtitle;

    if (time_text.empty()) {
        std::vector<limestone::Origin> origins;
        for (const limestone::NearbyStop& stop : nearby) {
            origins.push_back(limestone::Origin{stop.stop, stop.seconds});
        }

        limestone::SweepOptions options;
        options.threads = threads;
        const limestone::SweepResult result =
            limestone::run_sweep(timetable, transfers, origins, options);

        for (std::size_t s = 0; s != minutes.size(); ++s) {
            const limestone::StopAccess& access = result.stops[s];
            if (access.median_seconds >= 0) {
                minutes[s] = (access.median_seconds + 30) / 60;
            }
        }
        subtitle = "median travel time across " + std::to_string(result.departures) +
                   " departures, 06:00 to 23:59, " + date_text;
    } else {
        const std::optional<int> departure = limestone::parse_gtfs_time(time_text);
        if (!departure) {
            throw std::runtime_error("time must be HH:MM, got " + time_text);
        }

        limestone::Query query;
        query.target = limestone::kNoTarget;
        query.departure = *departure;
        for (const limestone::NearbyStop& stop : nearby) {
            query.origins.push_back(limestone::Origin{stop.stop, *departure + stop.seconds});
        }

        const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
        for (std::size_t s = 0; s != minutes.size(); ++s) {
            if (result.best[s] != limestone::kUnreachable) {
                minutes[s] = (result.best[s] - query.departure + 30) / 60;
            }
        }
        subtitle = "leaving at " + limestone::format_hhmm(*departure) + ", " + date_text;
    }

    std::vector<limestone::MapPoint> points;
    int unreachable = 0;
    for (std::size_t s = 0; s != feed.stops.size(); ++s) {
        const limestone::Stop& stop = feed.stops[s];
        if (stop.lat == 0.0 && stop.lon == 0.0) {
            continue;
        }
        points.push_back(limestone::MapPoint{stop.lat, stop.lon, minutes[s]});
        if (minutes[s] < 0) {
            ++unreachable;
        }
    }

    limestone::write_svg_map(output_path, points, lat, lon, "Kingston Transit travel time",
                             subtitle);

    std::cout << "stops plotted: " << points.size() << '\n'
              << "unreachable:   " << unreachable << '\n'
              << "written to:    " << output_path << '\n';
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return print_usage();
    }

    const std::string command = argv[1];

    try {
        if (command == "--version") {
            std::cout << "limestone " << limestone::version() << '\n';
            return 0;
        }
        if (command == "info" && argc == 3) {
            return run_info(argv[2]);
        }
        if (command == "stops" && argc == 4) {
            return run_stops(argv[2], argv[3]);
        }
        if (command == "route" && argc == 7) {
            return run_route(argv[2], argv[3], argv[4], argv[5], argv[6]);
        }

        if (command == "reach" && argc == 6) {
            return run_reach(argv[2], argv[3], argv[4], argv[5]);
        }

        if (command == "sweep" && (argc == 6 || argc == 7)) {
            const int threads = argc == 7 ? std::atoi(argv[6]) : 1;
            return run_sweep_command(argv[2], argv[3], argv[4], argv[5], threads);
        }

                if (command == "map" && argc >= 6 && argc <= 8) {
            std::string time_text;
            int threads = 4;
            for (int i = 6; i != argc; ++i) {
                const std::string argument = argv[i];
                if (argument.find(':') != std::string::npos) {
                    time_text = argument;
                } else {
                    threads = std::atoi(argument.c_str());
                }
            }
            return run_map(argv[2], argv[3], argv[4], argv[5], time_text, threads);
        }

    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return print_usage();
}