#include "limestone/raptor.hpp"

#include <algorithm>

namespace limestone {
namespace {

// Assumes trips within a pattern never overtake each other, so departures at
// any one position are sorted and the first catchable trip can be bisected.
int earliest_trip(const Timetable& timetable, const Pattern& pattern, int position, int ready) {
    int low = 0;
    int high = pattern.trip_count;
    while (low < high) {
        const int mid = low + (high - low) / 2;
        if (timetable.departure(pattern, mid, position) < ready) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low < pattern.trip_count ? low : -1;
}

}

RaptorResult run_raptor(const Timetable& timetable, const Query& query) {
    const int stop_count = timetable.stop_count;

    RaptorResult result;
    result.rounds.assign(query.max_trips + 1, std::vector<Label>(stop_count));

    std::vector<int> best(stop_count, kUnreachable);
    std::vector<char> is_marked(stop_count, 0);
    std::vector<int> marked;
    std::vector<int> pattern_start(timetable.patterns.size(), -1);
    std::vector<int> queue;

    result.rounds[0][query.source].arrival = query.departure;
    best[query.source] = query.departure;
    marked.push_back(query.source);
    is_marked[query.source] = 1;

    for (int k = 1; k <= query.max_trips && !marked.empty(); ++k) {
        const std::vector<Label>& previous = result.rounds[k - 1];
        std::vector<Label>& current = result.rounds[k];

        // Each round starts from the last, so rounds[k] holds the best known
        // arrival at every stop using at most k trips.
        current = previous;

        queue.clear();
        for (const int stop : marked) {
            is_marked[stop] = 0;
            for (int i = timetable.stop_pattern_offsets[stop];
                 i < timetable.stop_pattern_offsets[stop + 1]; ++i) {
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

            for (int position = pattern_start[p]; position < pattern.stop_count; ++position) {
                const int stop = timetable.stop_at(pattern, position);

                if (trip != -1) {
                    const int arrival = timetable.arrival(pattern, trip, position);
                    if (arrival < std::min(best[stop], best[query.target])) {
                        current[stop] = Label{arrival, p, trip, board};
                        best[stop] = arrival;
                        if (!is_marked[stop]) {
                            is_marked[stop] = 1;
                            marked.push_back(stop);
                        }
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
    }

    int best_so_far = kUnreachable;
    for (int k = 1; k <= query.max_trips; ++k) {
        const int arrival = result.rounds[k][query.target].arrival;
        if (arrival < best_so_far) {
            result.options.push_back(Option{k, arrival});
            best_so_far = arrival;
        }
    }

    return result;
}

}