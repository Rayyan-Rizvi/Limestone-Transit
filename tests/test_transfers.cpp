#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "limestone/feed.hpp"
#include "limestone/journey.hpp"
#include "limestone/raptor.hpp"
#include "limestone/timetable.hpp"
#include "limestone/transfers.hpp"

namespace {

constexpr int kDate = 20261014;

int hm(int hours, int minutes) {
    return hours * 3600 + minutes * 60;
}

void add_trip(limestone::Feed& feed, const std::string& route,
              const std::vector<std::pair<std::string, int>>& calls) {
    limestone::Trip trip;
    trip.route = feed.route_ids.lookup(route);
    trip.service = 0;
    trip.first_stop_time = static_cast<int>(feed.stop_times.size());
    trip.stop_time_count = static_cast<int>(calls.size());

    for (const auto& [stop_id, time] : calls) {
        feed.stop_times.push_back(limestone::StopTime{feed.stop_ids.lookup(stop_id), time, time});
    }

    feed.trip_ids.intern("T" + std::to_string(feed.trips.size()));
    feed.trips.push_back(trip);
}

// Four stops on one north-south street. B2 is 100 m from B, close enough to
// walk; every other pair is over a kilometre apart.
limestone::Feed walking_network() {
    limestone::Feed feed;
    feed.service_ids.intern("DAILY");
    feed.service_dates.push_back({kDate});

    for (const std::string route : {"X", "Y"}) {
        feed.route_ids.intern(route);
        feed.routes.push_back(limestone::Route{route, route});
    }

    const std::vector<std::pair<std::string, double>> stops = {
        {"A", 44.2000}, {"B", 44.2100}, {"B2", 44.2109}, {"D", 44.2300}};
    for (const auto& [id, lat] : stops) {
        feed.stop_ids.intern(id);
        feed.stops.push_back(limestone::Stop{id, lat, -76.5000});
    }

    add_trip(feed, "X", {{"A", hm(8, 0)}, {"B", hm(8, 10)}});
    add_trip(feed, "Y", {{"B2", hm(8, 15)}, {"D", hm(8, 30)}});
    return feed;
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

TEST(Haversine, MeasuresKnownDistance) {
    EXPECT_NEAR(limestone::haversine_metres(44.2253, -76.4951, 44.2310, -76.4870), 904.6, 5.0);
}

TEST(Haversine, IsZeroForSamePoint) {
    EXPECT_DOUBLE_EQ(limestone::haversine_metres(44.2, -76.5, 44.2, -76.5), 0.0);
}

TEST(Transfers, ConnectsOnlyNearbyStops) {
    const limestone::Feed feed = walking_network();
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    const int b = feed.stop_ids.lookup("B");
    ASSERT_EQ(transfers.offsets[b + 1] - transfers.offsets[b], 1);

    const limestone::Footpath& path = transfers.paths[transfers.offsets[b]];
    EXPECT_EQ(path.to, feed.stop_ids.lookup("B2"));
    EXPECT_EQ(path.seconds, 109);
    EXPECT_EQ(transfers.size(), 2u);
}

TEST(RaptorWalking, NeedsFootpathToReachNearbyStop) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);
    const limestone::Query query = make_query(feed, "A", "D", hm(7, 55));

    EXPECT_TRUE(limestone::run_raptor(timetable, query).options.empty());

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
    ASSERT_EQ(result.options.size(), 1u);
    EXPECT_EQ(result.options[0].trips, 2);
    EXPECT_EQ(result.options[0].arrival, hm(8, 30));
}

TEST(RaptorWalking, RecordsWhereAWalkStarted) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    const limestone::RaptorResult result =
        limestone::run_raptor(timetable, make_query(feed, "A", "D", hm(7, 55)), &transfers);

    const limestone::Label& label = result.rounds[1][feed.stop_ids.lookup("B2")];
    EXPECT_EQ(label.walk_from, feed.stop_ids.lookup("B"));
}

TEST(RaptorWalking, WalksFromOriginToNearbyBay) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    const limestone::RaptorResult result =
        limestone::run_raptor(timetable, make_query(feed, "B", "D", hm(8, 0)), &transfers);

    ASSERT_EQ(result.options.size(), 1u);
    EXPECT_EQ(result.options[0].trips, 1);
    EXPECT_EQ(result.options[0].arrival, hm(8, 30));
}

TEST(Journey, ReconstructsRideWalkRide) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);
    const limestone::Query query = make_query(feed, "A", "D", hm(7, 55));

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
    const limestone::Journey journey = limestone::reconstruct_journey(timetable, result, query, 2);

    ASSERT_EQ(journey.legs.size(), 3u);
    EXPECT_EQ(journey.legs[0].kind, limestone::LegKind::Ride);
    EXPECT_EQ(journey.legs[0].from_stop, feed.stop_ids.lookup("A"));
    EXPECT_EQ(journey.legs[0].depart, hm(8, 0));
    EXPECT_EQ(journey.legs[1].kind, limestone::LegKind::Walk);
    EXPECT_EQ(journey.legs[1].to_stop, feed.stop_ids.lookup("B2"));
    EXPECT_EQ(journey.legs[2].kind, limestone::LegKind::Ride);
    EXPECT_EQ(journey.legs[2].depart, hm(8, 15));
    EXPECT_EQ(journey.legs[2].arrive, hm(8, 30));
}

TEST(Journey, StartsWithWalkToNearbyBay) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);
    const limestone::Query query = make_query(feed, "B", "D", hm(8, 0));

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
    const limestone::Journey journey = limestone::reconstruct_journey(timetable, result, query, 1);

    ASSERT_EQ(journey.legs.size(), 2u);
    EXPECT_EQ(journey.legs[0].kind, limestone::LegKind::Walk);
    EXPECT_EQ(journey.legs[0].depart, hm(8, 0));
    EXPECT_EQ(journey.legs[0].arrive, hm(8, 0) + 109);
    EXPECT_EQ(journey.legs[1].route, feed.route_ids.lookup("Y"));
}

TEST(Journey, LegsConnectEndToEnd) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);
    const limestone::Query query = make_query(feed, "A", "D", hm(7, 55));

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);
    const limestone::Journey journey = limestone::reconstruct_journey(timetable, result, query, 2);

    EXPECT_EQ(journey.legs.front().from_stop, query.source);
    EXPECT_EQ(journey.legs.back().to_stop, query.target);
    for (std::size_t i = 0; i + 1 != journey.legs.size(); ++i) {
        EXPECT_EQ(journey.legs[i].to_stop, journey.legs[i + 1].from_stop);
    }
}