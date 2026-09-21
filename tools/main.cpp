#include <chrono>
#include <iostream>
#include <string>

#include "limestone/loader.hpp"
#include "limestone/version.hpp"

namespace {

int print_usage() {
    std::cerr << "usage:\n"
              << "  limestone info FEED_DIR\n"
              << "  limestone stops FEED_DIR QUERY\n"
              << "  limestone --version\n";
    return 2;
}

int run_info(const std::string& directory) {
    const auto started = std::chrono::steady_clock::now();

    limestone::LoadStats stats;
    const limestone::Feed feed = limestone::load_feed(directory, &stats);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);

    std::cout << "feed:        " << directory << '\n'
              << "stops:       " << feed.stops.size() << '\n'
              << "routes:      " << feed.routes.size() << '\n'
              << "trips:       " << feed.trips.size() << '\n'
              << "stop times:  " << feed.stop_times.size() << '\n'
              << "services:    " << feed.service_ids.size() << '\n'
              << "load time:   " << elapsed.count() << " ms\n";

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
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return print_usage();
}