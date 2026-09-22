#pragma once

#include <limits>
#include <vector>

#include "limestone/timetable.hpp"
#include "limestone/transfers.hpp"

namespace limestone {

constexpr int kUnreachable = std::numeric_limits<int>::max();
constexpr int kNoTarget = -1;

// A stop the search may start from, and when the rider can be there.
struct Origin {
    int stop = 0;
    int departure = 0;
};

// With origins empty, the search starts from source at departure. With
// target set to kNoTarget it computes arrivals at every stop instead of
// pruning toward a single destination.
struct Query {
    int source = 0;
    int target = 0;
    int departure = 0;
    int max_trips = 5;
    int transfer_buffer = 60;
    std::vector<Origin> origins;
};

// How a stop was reached in a round: either by riding a pattern's trip from
// board_position, or on foot from walk_from.
struct Label {
    int arrival = kUnreachable;
    int pattern = -1;
    int trip = -1;
    int board_position = -1;
    int walk_from = -1;
};

struct Option {
    int trips = 0;
    int arrival = 0;
};

struct RaptorResult {
    std::vector<std::vector<Label>> rounds;
    std::vector<Option> options;
    std::vector<int> best;
};

RaptorResult run_raptor(const Timetable& timetable, const Query& query,
                        const Transfers* transfers = nullptr);

}