#include <gtest/gtest.h>

#include "limestone/feed.hpp"
#include "limestone/interner.hpp"

TEST(Interner, AssignsStableIndices) {
    limestone::Interner interner;
    const int a = interner.intern("S02077");
    const int b = interner.intern("00799");

    EXPECT_EQ(interner.intern("S02077"), a);
    EXPECT_NE(a, b);
    EXPECT_EQ(interner.name(a), "S02077");
    EXPECT_EQ(interner.size(), 2u);
}

TEST(Interner, ReportsUnknownIds) {
    limestone::Interner interner;
    interner.intern("S1");
    EXPECT_EQ(interner.lookup("S1"), 0);
    EXPECT_EQ(interner.lookup("nope"), limestone::Interner::kMissing);
}

TEST(Feed, ServiceRunsOnlyOnListedDates) {
    limestone::Feed feed;
    feed.service_dates.push_back({20261013, 20261014, 20261015});

    EXPECT_TRUE(feed.service_runs_on(0, 20261014));
    EXPECT_FALSE(feed.service_runs_on(0, 20261012));
    EXPECT_FALSE(feed.service_runs_on(1, 20261014));
}

TEST(Feed, FindsStopsByName) {
    limestone::Feed feed;
    feed.stops.push_back({"Union St at University Ave", 44.225, -76.495});
    feed.stops.push_back({"Cataraqui Centre", 44.255, -76.572});

    EXPECT_EQ(feed.find_stop_by_name("cataraqui centre"), 1);
    EXPECT_EQ(feed.find_stop_by_name("union"), 0);
    EXPECT_EQ(feed.find_stop_by_name("not a stop"), -1);
}