#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "limestone/feed.hpp"
#include "limestone/raptor.hpp"
#include "limestone/timetable.hpp"

namespace {

constexpr int kDate = 20261014;

int hm(int hours, int minutes) {
    return hours * 3600 + minutes * 60;
}

class NetworkBuilder {
public:
    NetworkBuilder() {
        feed_.service_ids.intern("DAILY");
        feed_.service_dates.push_back({kDate});
        feed_.service_ids.intern("NEVER");
        feed_.service_dates.push_back({});
    }

    int stop(const std::string& id) {
        const int index = feed_.stop_ids.intern(id);
        if (index == static_cast<int>(feed_.stops.size())) {
            feed_.stops.push_back(limestone::Stop{id, 0.0, 0.0});
        }
        return index;
    }

    void trip(const std::string& route_id, const std::vector<std::pair<std::string, int>>& calls,
              const std::string& service = "DAILY") {
        limestone::Trip trip;
        trip.route = route(route_id);
        trip.service = feed_.service_ids.lookup(service);
        trip.first_stop_time = static_cast<int>(feed_.stop_times.size());
        trip.stop_time_count = static_cast<int>(calls.size());

        for (const auto& [stop_id, time] : calls) {
            feed_.stop_times.push_back(limestone::StopTime{stop(stop_id), time, time});
        }

        feed_.trip_ids.intern("T" + std::to_string(feed_.trips.size()));
        feed_.trips.push_back(trip);
    }

    const limestone::Feed& feed() const { return feed_; }

private:
    int route(const std::string& id) {
        const int index = feed_.route_ids.intern(id);
        if (index == static_cast<int>(feed_.routes.size())) {
            feed_.routes.push_back(limestone::Route{id, id});
        }
        return index;
    }

    limestone::Feed feed_;
};

// A to D is possible directly on R3 (arrive 09:40) or on R1 to B then R2 to D
// (arrive 08:35), so one extra bus saves over an hour.
NetworkBuilder sample_network() {
    NetworkBuilder network;
    network.trip("R1", {{"A", hm(8, 0)}, {"B", hm(8, 10)}, {"C", hm(8, 20)}});
    network.trip("R1", {{"A", hm(8, 30)}, {"B", hm(8, 40)}, {"C", hm(8, 50)}});
    network.trip("R2", {{"B", hm(8, 20)}, {"D", hm(8, 35)}});
    network.trip("R3", {{"A", hm(9, 0)}, {"D", hm(9, 40)}});
    return network;
}

limestone::Query make_query(const limestone::Feed& feed, const std::string& from,
                            const std::string& to, int departure) {
    limestone::Query query;
    query.source = feed.stop_ids.lookup(from);
    query.target = feed.stop_ids.lookup(to);
    query.departure = departure;
    return query;
}

}

TEST(Timetable, SplitsRouteVariantsIntoSeparatePatterns) {
    NetworkBuilder network = sample_network();
    network.trip("R1", {{"A", hm(10, 0)}, {"C", hm(10, 15)}});

    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);
    EXPECT_EQ(timetable.patterns.size(), 4u);
}

TEST(Timetable, ExcludesTripsNotRunningOnDate) {
    NetworkBuilder network = sample_network();
    network.trip("R3", {{"A", hm(8, 0)}, {"D", hm(8, 5)}}, "NEVER");

    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);
    EXPECT_EQ(timetable.pattern_trips.size(), 4u);
}

TEST(Timetable, IndexesPatternsServingEachStop) {
    const NetworkBuilder network = sample_network();
    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);

    const int b = network.feed().stop_ids.lookup("B");
    EXPECT_EQ(timetable.stop_pattern_offsets[b + 1] - timetable.stop_pattern_offsets[b], 2);
}

TEST(Raptor, FindsDirectAndTransferOptions) {
    const NetworkBuilder network = sample_network();
    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);

    const limestone::RaptorResult result =
        limestone::run_raptor(timetable, make_query(network.feed(), "A", "D", hm(7, 55)));

    ASSERT_EQ(result.options.size(), 2u);
    EXPECT_EQ(result.options[0].trips, 1);
    EXPECT_EQ(result.options[0].arrival, hm(9, 40));
    EXPECT_EQ(result.options[1].trips, 2);
    EXPECT_EQ(result.options[1].arrival, hm(8, 35));
}

TEST(Raptor, RespectsTransferBuffer) {
    const NetworkBuilder network = sample_network();
    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);

    limestone::Query query = make_query(network.feed(), "A", "D", hm(7, 55));
    query.transfer_buffer = 15 * 60;

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query);
    ASSERT_EQ(result.options.size(), 1u);
    EXPECT_EQ(result.options[0].arrival, hm(9, 40));
}

TEST(Raptor, IgnoresTripsThatAlreadyLeft) {
    const NetworkBuilder network = sample_network();
    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);

    const limestone::RaptorResult result =
        limestone::run_raptor(timetable, make_query(network.feed(), "A", "D", hm(8, 5)));

    ASSERT_EQ(result.options.size(), 1u);
    EXPECT_EQ(result.options[0].arrival, hm(9, 40));
}

TEST(Raptor, RecordsTheTripThatReachedTheTarget) {
    const NetworkBuilder network = sample_network();
    const limestone::Feed& feed = network.feed();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);

    const limestone::Query query = make_query(feed, "A", "D", hm(7, 55));
    const limestone::RaptorResult result = limestone::run_raptor(timetable, query);

    const limestone::Label& label = result.rounds[2][query.target];
    const limestone::Pattern& pattern = timetable.patterns[label.pattern];
    EXPECT_EQ(pattern.route, feed.route_ids.lookup("R2"));
    EXPECT_EQ(timetable.stop_at(pattern, label.board_position), feed.stop_ids.lookup("B"));
}

TEST(Raptor, ReportsNothingForUnreachableStop) {
    NetworkBuilder network = sample_network();
    network.stop("E");
    const limestone::Timetable timetable = limestone::build_timetable(network.feed(), kDate);

    const limestone::RaptorResult result =
        limestone::run_raptor(timetable, make_query(network.feed(), "A", "E", hm(7, 55)));
    EXPECT_TRUE(result.options.empty());
}