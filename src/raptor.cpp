#include "limestone/raptor.hpp"

#include <algorithm>
#include <utility>

namespace limestone {
namespace {

// Assumes trips within a pattern never overtake each other, so departures at
// any one position are sorted and the first catchable trip can be bisected.
int earliest_trip(const Timetable& timetable, const Pattern& pattern, int position, int ready) {
    int low = 0;
    int high = pattern.trip_count;
    while (low != high) {
        const int mid = low + (high - low) / 2;
        if (timetable.departure(pattern, mid, position) >= ready) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return low != pattern.trip_count ? low : -1;
}

int target_bound(const std::vector<int>& best, int target) {
    return target == kNoTarget ? kUnreachable : best[target];
}

void mark(int stop, std::vector<char>& is_marked, std::vector<int>& marked) {
    if (!is_marked[stop]) {
        is_marked[stop] = 1;
        marked.push_back(stop);
    }
}

// Walks start only from stops reached by riding in this round, or from the
// origins, never from the far end of another walk.
void relax_footpaths(const Transfers& transfers, int target, std::vector<Label>& labels,
                     std::vector<int>& best, std::vector<char>& is_marked,
                     std::vector<int>& marked) {
    const std::size_t reached_by_riding = marked.size();

    for (std::size_t i = 0; i != reached_by_riding; ++i) {
        const int from = marked[i];
        if (labels[from].walk_from != -1) {
            continue;
        }

        const int leave_at = labels[from].arrival;
        for (int j = transfers.offsets[from]; j != transfers.offsets[from + 1]; ++j) {
            const Footpath& path = transfers.paths[j];
            const int arrival = leave_at + path.seconds;
            if (arrival >= std::min(best[path.to], target_bound(best, target))) {
                continue;
            }

            labels[path.to] = Label{arrival, -1, -1, -1, from};
            best[path.to] = arrival;
            mark(path.to, is_marked, marked);
        }
    }
}

}

RaptorResult run_raptor(const Timetable& timetable, const Query& query,
                        const Transfers* transfers) {
    const int stop_count = timetable.stop_count;

    RaptorResult result;
    result.rounds.assign(query.max_trips + 1, std::vector<Label>(stop_count));

    std::vector<int> best(stop_count, kUnreachable);
    std::vector<char> is_marked(stop_count, 0);
    std::vector<int> marked;
    std::vector<int> pattern_start(timetable.patterns.size(), -1);
    std::vector<int> queue;

    std::vector<Label>& origin_labels = result.rounds[0];
    const auto seed = [&](int stop, int departure) {
        if (departure >= origin_labels[stop].arrival) {
            return;
        }
        origin_labels[stop].arrival = departure;
        best[stop] = departure;
        mark(stop, is_marked, marked);
    };

    if (query.origins.empty()) {
        seed(query.source, query.departure);
    } else {
        for (const Origin& origin : query.origins) {
            seed(origin.stop, origin.departure);
        }
    }

    if (transfers) {
        relax_footpaths(*transfers, query.target, origin_labels, best, is_marked, marked);
    }

    for (int k = 1; k != query.max_trips + 1 && !marked.empty(); ++k) {
        const std::vector<Label>& previous = result.rounds[k - 1];
        std::vector<Label>& current = result.rounds[k];

        // Each round starts from the last, so rounds[k] holds the best known
        // arrival at every stop using at most k trips.
        current = previous;

        queue.clear();
        for (const int stop : marked) {
            is_marked[stop] = 0;
            const int first = timetable.stop_pattern_offsets[stop];
            const int last = timetable.stop_pattern_offsets[stop + 1];
            for (int i = first; i != last; ++i) {
                const PatternStop& entry = timetable.stop_patterns[i];
                int& start = pattern_start[entry.pattern];
                if (start == -1) {
                    start = entry.position;
                    queue.push_back(entry.pattern);
                } else {
                    start = std::min(start, entry.position);
                }
            }
        }
        marked.clear();

        for (const int p : queue) {
            const Pattern& pattern = timetable.patterns[p];
            int trip = -1;
            int board = -1;

            for (int position = pattern_start[p]; position != pattern.stop_count; ++position) {
                const int stop = timetable.stop_at(pattern, position);

                if (trip != -1) {
                    const int arrival = timetable.arrival(pattern, trip, position);
                    if (arrival < std::min(best[stop], target_bound(best, query.target))) {
                        current[stop] = Label{arrival, p, trip, board};
                        best[stop] = arrival;
                        mark(stop, is_marked, marked);
                    }
                }

                const int reached = previous[stop].arrival;
                if (reached == kUnreachable) {
                    continue;
                }

                const int ready = k == 1 ? reached : reached + query.transfer_buffer;
                if (trip == -1 || ready <= timetable.departure(pattern, trip, position)) {
                    const int candidate = earliest_trip(timetable, pattern, position, ready);
                    if (candidate != -1 && (trip == -1 || candidate < trip)) {
                        trip = candidate;
                        board = position;
                    }
                }
            }

            pattern_start[p] = -1;
        }

        if (transfers) {
            relax_footpaths(*transfers, query.target, current, best, is_marked, marked);
        }
    }

    if (query.target != kNoTarget) {
        int best_so_far = kUnreachable;
        for (int k = 0; k != query.max_trips + 1; ++k) {
            const int arrival = result.rounds[k][query.target].arrival;
            if (arrival < best_so_far) {
                result.options.push_back(Option{k, arrival});
                best_so_far = arrival;
            }
        }
    }

    result.best = std::move(best);
    return result;
}

}