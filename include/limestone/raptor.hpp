#pragma once

#include <limits>
#include <vector>

#include "limestone/timetable.hpp"

namespace limestone {

constexpr int kUnreachable = std::numeric_limits<int>::max();

struct Query {
    int source = 0;
    int target = 0;
    int departure = 0;
    int max_trips = 5;
    int transfer_buffer = 60;
};

// How a stop was reached: which pattern and trip, boarded at which position.
struct Label {
    int arrival = kUnreachable;
    int pattern = -1;
    int trip = -1;
    int board_position = -1;
};

struct Option {
    int trips = 0;
    int arrival = 0;
};

struct RaptorResult {
    std::vector<std::vector<Label>> rounds;
    std::vector<Option> options;
};

RaptorResult run_raptor(const Timetable& timetable, const Query& query);

}