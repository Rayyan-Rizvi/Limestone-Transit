#pragma once

#include <vector>

#include "limestone/raptor.hpp"
#include "limestone/timetable.hpp"
#include "limestone/transfers.hpp"

namespace limestone {

struct SweepOptions {
    int first_departure = 6 * 3600;
    int last_departure = 23 * 3600 + 59 * 60;
    int step_seconds = 60;
    int max_trips = 5;
    int transfer_buffer = 60;
    int threads = 1;
};

struct StopAccess {
    int samples = 0;
    int reached = 0;
    int median_seconds = -1;
    int best_seconds = -1;
};

struct SweepResult {
    int departures = 0;
    std::vector<StopAccess> stops;
};

// Runs a one-to-all query for every departure in the window and summarises,
// per stop, how often and how quickly it can be reached.
SweepResult run_sweep(const Timetable& timetable, const Transfers& transfers,
                      const std::vector<Origin>& origins, const SweepOptions& options);

}