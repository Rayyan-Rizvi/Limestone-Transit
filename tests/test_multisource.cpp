#include <gtest/gtest.h>

#include <vector>

#include "limestone/journey.hpp"
#include "limestone/raptor.hpp"
#include "limestone/timetable.hpp"
#include "limestone/transfers.hpp"
#include "walking_network.hpp"

using limestone_test::hm;
using limestone_test::kDate;
using limestone_test::walking_network;

TEST(StopsNear, FindsStopsWithinRadius) {
    const limestone::Feed feed = walking_network();
    const std::vector<limestone::NearbyStop> nearby =
        limestone::stops_near(feed, 44.2100, -76.5000, 150.0);

    ASSERT_EQ(nearby.size(), 2u);
    EXPECT_EQ(nearby[0].stop, feed.stop_ids.lookup("B"));
    EXPECT_EQ(nearby[0].seconds, 30);
    EXPECT_EQ(nearby[1].stop, feed.stop_ids.lookup("B2"));
    EXPECT_EQ(nearby[1].seconds, 109);
}

TEST(MultiSource, StartsFromWhicheverOriginIsBest) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);

    limestone::Query query;
    query.target = feed.stop_ids.lookup("D");
    query.origins = {{feed.stop_ids.lookup("A"), hm(7, 55)},
                     {feed.stop_ids.lookup("B2"), hm(8, 12)}};

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query);
    ASSERT_EQ(result.options.size(), 1u);
    EXPECT_EQ(result.options[0].trips, 1);
    EXPECT_EQ(result.options[0].arrival, hm(8, 30));
}

TEST(MultiSource, ReconstructsJourneyFromSecondaryOrigin) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);

    limestone::Query query;
    query.target = feed.stop_ids.lookup("D");
    query.origins = {{feed.stop_ids.lookup("A"), hm(7, 55)},
                     {feed.stop_ids.lookup("B2"), hm(8, 12)}};

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query);
    const limestone::Journey journey = limestone::reconstruct_journey(timetable, result, query, 1);

    ASSERT_EQ(journey.legs.size(), 1u);
    EXPECT_EQ(journey.legs[0].from_stop, feed.stop_ids.lookup("B2"));
    EXPECT_EQ(journey.legs[0].depart, hm(8, 15));
}

TEST(OneToAll, ComputesArrivalAtEveryStop) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    const limestone::Transfers transfers = limestone::build_transfers(feed);

    limestone::Query query;
    query.source = feed.stop_ids.lookup("A");
    query.target = limestone::kNoTarget;
    query.departure = hm(7, 55);

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query, &transfers);

    EXPECT_TRUE(result.options.empty());
    EXPECT_EQ(result.best[feed.stop_ids.lookup("A")], hm(7, 55));
    EXPECT_EQ(result.best[feed.stop_ids.lookup("B")], hm(8, 10));
    EXPECT_EQ(result.best[feed.stop_ids.lookup("B2")], hm(8, 10) + 109);
    EXPECT_EQ(result.best[feed.stop_ids.lookup("D")], hm(8, 30));
}

TEST(OneToAll, LeavesUnreachableStopsUnreachable) {
    const limestone::Feed feed = walking_network();
    const limestone::Timetable timetable = limestone::build_timetable(feed, kDate);

    limestone::Query query;
    query.source = feed.stop_ids.lookup("A");
    query.target = limestone::kNoTarget;
    query.departure = hm(7, 55);

    const limestone::RaptorResult result = limestone::run_raptor(timetable, query);
    EXPECT_EQ(result.best[feed.stop_ids.lookup("D")], limestone::kUnreachable);
}