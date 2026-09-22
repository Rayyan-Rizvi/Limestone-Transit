#include "limestone/journey.hpp"

#include <algorithm>
#include <stdexcept>

namespace limestone {

Journey reconstruct_journey(const Timetable& timetable, const RaptorResult& result,
                            const Query& query, int trips) {
    Journey journey;
    journey.trips = trips;
    journey.arrival = result.rounds[trips][query.target].arrival;
    if (journey.arrival == kUnreachable) {
        throw std::runtime_error("no journey reaches the target with that many trips");
    }

    int stop = query.target;
    int round = trips;

    // Every step moves to an earlier round or follows a single walk, so a
    // valid label chain finishes well inside this bound.
    const int max_steps = 4 * (trips + 1) + 4;

    for (int step = 0; step != max_steps; ++step) {
        const Label& label = result.rounds[round][stop];

        if (label.walk_from != -1) {
            Leg leg;
            leg.kind = LegKind::Walk;
            leg.from_stop = label.walk_from;
            leg.to_stop = stop;
            leg.depart = result.rounds[round][label.walk_from].arrival;
            leg.arrive = label.arrival;
            journey.legs.push_back(leg);

            stop = label.walk_from;
            continue;
        }

        // Only seeded origins carry a label that is neither a ride nor a walk.
        if (label.pattern == -1) {
            if (label.arrival == kUnreachable) {
                throw std::runtime_error("journey reconstruction reached an unreached stop");
            }
            std::reverse(journey.legs.begin(), journey.legs.end());
            return journey;
        }

        if (round == 0) {
            throw std::runtime_error("found a ride label in round zero");
        }

        const Pattern& pattern = timetable.patterns[label.pattern];

        Leg leg;
        leg.kind = LegKind::Ride;
        leg.from_stop = timetable.stop_at(pattern, label.board_position);
        leg.to_stop = stop;
        leg.depart = timetable.departure(pattern, label.trip, label.board_position);
        leg.arrive = label.arrival;
        leg.route = pattern.route;
        leg.trip = timetable.pattern_trips[pattern.first_trip + label.trip];
        journey.legs.push_back(leg);

        stop = leg.from_stop;
        round -= 1;
    }

    throw std::runtime_error("journey reconstruction did not reach the origin");
}

}