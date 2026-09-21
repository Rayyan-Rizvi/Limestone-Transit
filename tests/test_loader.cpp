#include <gtest/gtest.h>

#include <string>

#include "limestone/loader.hpp"

namespace {

std::string mini_feed_path() {
    return std::string(LIMESTONE_TEST_DATA) + "/mini";
}

}

TEST(Loader, LoadsStopsRoutesAndTrips) {
    const limestone::Feed feed = limestone::load_feed(mini_feed_path());

    EXPECT_EQ(feed.stops.size(), 3u);
    EXPECT_EQ(feed.routes.size(), 1u);
    EXPECT_EQ(feed.trips.size(), 2u);
    EXPECT_EQ(feed.stops[1].name, "Princess St, north side");
}

TEST(Loader, OrdersStopTimesBySequenceWithinTrip) {
    const limestone::Feed feed = limestone::load_feed(mini_feed_path());
    const limestone::Trip& trip = feed.trips[feed.trip_ids.lookup("T1")];

    ASSERT_EQ(trip.stop_time_count, 3);
    const limestone::StopTime* times = &feed.stop_times[trip.first_stop_time];
    EXPECT_EQ(times[0].arrival, 8 * 3600);
    EXPECT_EQ(times[1].arrival, 8 * 3600 + 5 * 60);
    EXPECT_EQ(times[2].arrival, 8 * 3600 + 12 * 60);
}

TEST(Loader, KeepsTimesPastMidnightOnServiceDay) {
    const limestone::Feed feed = limestone::load_feed(mini_feed_path());
    const limestone::Trip& trip = feed.trips[feed.trip_ids.lookup("T2")];

    const limestone::StopTime& last = feed.stop_times[trip.first_stop_time + trip.stop_time_count - 1];
    EXPECT_EQ(last.arrival, 25 * 3600 + 10 * 60);
}

TEST(Loader, SkipsRowsWithUnknownReferences) {
    limestone::LoadStats stats;
    limestone::load_feed(mini_feed_path(), &stats);
    EXPECT_EQ(stats.skipped_stop_times, 1);
}

TEST(Loader, ReadsServiceDatesAndIgnoresRemovals) {
    const limestone::Feed feed = limestone::load_feed(mini_feed_path());
    const int service = feed.service_ids.lookup("WEEKDAY");

    EXPECT_TRUE(feed.service_runs_on(service, 20261013));
    EXPECT_FALSE(feed.service_runs_on(service, 20261012));
}

TEST(Loader, ThrowsOnMissingDirectory) {
    EXPECT_THROW(limestone::load_feed("/tmp/limestone_no_feed_here"), std::runtime_error);
}