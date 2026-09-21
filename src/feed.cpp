#include "limestone/feed.hpp"

#include <algorithm>
#include <cctype>

namespace limestone {
namespace {

std::string fold(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

}

bool Feed::service_runs_on(int service, int date) const {
    if (service < 0 || service >= static_cast<int>(service_dates.size())) {
        return false;
    }
    const std::vector<int>& dates = service_dates[service];
    return std::binary_search(dates.begin(), dates.end(), date);
}

std::vector<int> Feed::search_stops(const std::string& query, std::size_t limit) const {
    const std::string needle = fold(query);
    std::vector<int> matches;

    for (std::size_t i = 0; i < stops.size(); ++i) {
        if (fold(stops[i].name).find(needle) != std::string::npos) {
            matches.push_back(static_cast<int>(i));
            if (matches.size() >= limit) {
                break;
            }
        }
    }
    return matches;
}

// Exact name wins over a substring hit so that a stop whose name is contained
// in several others is still reachable by typing it in full.
int Feed::find_stop_by_name(const std::string& query) const {
    const std::string needle = fold(query);

    for (std::size_t i = 0; i < stops.size(); ++i) {
        if (fold(stops[i].name) == needle) {
            return static_cast<int>(i);
        }
    }

    const std::vector<int> matches = search_stops(query, 1);
    return matches.empty() ? -1 : matches[0];
}

}