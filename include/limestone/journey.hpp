#pragma once

#include <vector>

#include "limestone/raptor.hpp"
#include "limestone/timetable.hpp"

namespace limestone {

enum class LegKind { Ride, Walk };

struct Leg {
    LegKind kind = LegKind::Ride;
    int from_stop = 0;
    int to_stop = 0;
    int depart = 0;
    int arrive = 0;
    int route = -1;
    int trip = -1;
};

struct Journey {
    int trips = 0;
    int arrival = 0;
    std::vector<Leg> legs;
};

// Follows round labels back from the target to recover the legs of the
// fastest journey using at most `trips` vehicles.
Journey reconstruct_journey(const Timetable& timetable, const RaptorResult& result,
                            const Query& query, int trips);

}