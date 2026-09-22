#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "limestone/loader.hpp"
#include "limestone/raptor.hpp"
#include "limestone/time.hpp"
#include "limestone/timetable.hpp"
#include "limestone/version.hpp"
#include "limestone/transfers.hpp"

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
              << "  limestone route FEED_DIR FROM_STOP_ID TO_STOP_ID YYYYMMDD HH:MM\n"
              << "  limestone --version\n";
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

    std::cout << "feed:        " << directory << '\n'
              << "stops:       " << feed.stops.size() << '\n'
              << "routes:      " << feed.routes.size() << '\n'
              << "trips:       " << feed.trips.size() << '\n'
              << "stop times:  " << feed.stop_times.size() << '\n'
              << "services:    " << feed.service_ids.size() << '\n'
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

int resolve_stop(const limestone::Feed& feed, const std::string& id) {
    const int index = feed.stop_ids.lookup(id);
    if (index == limestone::Interner::kMissing) {
        throw std::runtime_error("unknown stop id " + id + " (use the stops command to find ids)");
    }
    return index;
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

    std::cout << "from:      " << from << "  " << feed.stops[query.source].name << '\n'
              << "to:        " << to << "  " << feed.stops[query.target].name << '\n'
              << "departing: " << date << " at " << limestone::format_hhmm(query.departure) << '\n'
              << "patterns:  " << timetable.patterns.size() << " (built in " << build_ms << " ms)\n\n";

    if (result.options.empty()) {
        std::cout << "  no journey found\n";
    }
    for (const limestone::Option& option : result.options) {
        std::cout << "  " << option.trips << (option.trips == 1 ? " bus:    " : " buses:  ")
                  << "arrive " << limestone::format_hhmm(option.arrival) << '\n';
    }

    std::cout << "\nquery:     " << query_us << " microseconds\n";
    return result.options.empty() ? 1 : 0;
}

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
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return print_usage();
}