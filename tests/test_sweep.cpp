#include <gtest/gtest.h>

#include <vector>

#include "limestone/sweep.hpp"
#include "limestone/timetable.hpp"
#include "limestone/transfers.hpp"
#include "walking_network.hpp"

using limestone_test::hm;
using limestone_test::kDate;
using limestone_test::walking_network;

namespace {

struct Fixture {
    limestone::Feed feed = walking_network();
    limestone::Timetable timetable = limestone::build_timetable(feed, kDate);
    limestone::Transfers transfers = limestone::build_transfers(feed);
};

limestone::SweepOptions morning_options() {
    limestone::SweepOptions options;
    options.first_departure = hm(7, 30);
    options.last_departure = hm(8, 0);
    return options;
}

}

TEST(Sweep, CountsOneSamplePerDeparture) {
    const Fixture fixture;
    const std::vector<limestone::Origin> origins = {{fixture.feed.stop_ids.lookup("A"), 0}};

    const limestone::SweepResult result =
        limestone::run_sweep(fixture.timetable, fixture.transfers, origins, morning_options());

    EXPECT_EQ(result.departures, 31);
    EXPECT_EQ(result.stops[fixture.feed.stop_ids.lookup("D")].samples, 31);
}

TEST(Sweep, MeasuresTravelTimeFromEachDeparture) {
    const Fixture fixture;
    const std::vector<limestone::Origin> origins = {{fixture.feed.stop_ids.lookup("A"), 0}};

    const limestone::SweepResult result =
        limestone::run_sweep(fixture.timetable, fixture.transfers, origins, morning_options());
    const limestone::StopAccess& access = result.stops[fixture.feed.stop_ids.lookup("D")];

    // The only useful trip leaves A at 08:00 and reaches D at 08:30, so a
    // rider leaving at 07:30 waits half an hour and one leaving at 08:00 does not.
    EXPECT_EQ(access.reached, 31);
    EXPECT_EQ(access.best_seconds, 30 * 60);
    EXPECT_EQ(access.median_seconds, 45 * 60);
}

TEST(Sweep, RecordsUnreachableStopsAsNeverReached) {
    const Fixture fixture;
    const std::vector<limestone::Origin> origins = {{fixture.feed.stop_ids.lookup("D"), 0}};

    const limestone::SweepResult result =
        limestone::run_sweep(fixture.timetable, fixture.transfers, origins, morning_options());
    const limestone::StopAccess& access = result.stops[fixture.feed.stop_ids.lookup("A")];

    EXPECT_EQ(access.reached, 0);
    EXPECT_EQ(access.median_seconds, -1);
}

TEST(Sweep, AddsTheWalkFromTheQueryPoint) {
    const Fixture fixture;
    const std::vector<limestone::Origin> origins = {{fixture.feed.stop_ids.lookup("A"), 120}};

    limestone::SweepOptions options = morning_options();
    options.first_departure = hm(7, 58);
    options.last_departure = hm(7, 58);

    const limestone::SweepResult result =
        limestone::run_sweep(fixture.timetable, fixture.transfers, origins, options);

    // Leaving at 07:58 with a two minute walk means reaching A exactly at 08:00.
    EXPECT_EQ(result.stops[fixture.feed.stop_ids.lookup("D")].best_seconds, 32 * 60);
}

TEST(Sweep, ThreadCountDoesNotChangeResults) {
    const Fixture fixture;
    const std::vector<limestone::Origin> origins = {{fixture.feed.stop_ids.lookup("A"), 0}};

    limestone::SweepOptions single = morning_options();
    limestone::SweepOptions many = morning_options();
    many.threads = 4;

    const limestone::SweepResult a =
        limestone::run_sweep(fixture.timetable, fixture.transfers, origins, single);
    const limestone::SweepResult b =
        limestone::run_sweep(fixture.timetable, fixture.transfers, origins, many);

    ASSERT_EQ(a.stops.size(), b.stops.size());
    for (std::size_t i = 0; i != a.stops.size(); ++i) {
        EXPECT_EQ(a.stops[i].reached, b.stops[i].reached);
        EXPECT_EQ(a.stops[i].median_seconds, b.stops[i].median_seconds);
    }
}