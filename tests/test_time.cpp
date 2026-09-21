#include <gtest/gtest.h>

#include "limestone/time.hpp"

TEST(ParseGtfsTime, ParsesMorningTime) {
    EXPECT_EQ(limestone::parse_gtfs_time("08:30:00"), 30600);
}

TEST(ParseGtfsTime, ParsesTimePastMidnight) {
    EXPECT_EQ(limestone::parse_gtfs_time("25:10:00"), 90600);
}

TEST(ParseGtfsTime, AcceptsSingleDigitHour) {
    EXPECT_EQ(limestone::parse_gtfs_time("7:05:00"), 25500);
}

TEST(ParseGtfsTime, AcceptsMissingSeconds) {
    EXPECT_EQ(limestone::parse_gtfs_time("09:15"), 33300);
}

TEST(ParseGtfsTime, IgnoresSurroundingWhitespace) {
    EXPECT_EQ(limestone::parse_gtfs_time("  06:45:00 "), 24300);
}

TEST(ParseGtfsTime, RejectsJunk) {
    EXPECT_FALSE(limestone::parse_gtfs_time("").has_value());
    EXPECT_FALSE(limestone::parse_gtfs_time("abc").has_value());
    EXPECT_FALSE(limestone::parse_gtfs_time("08:70:00").has_value());
    EXPECT_FALSE(limestone::parse_gtfs_time("08:30:99").has_value());
    EXPECT_FALSE(limestone::parse_gtfs_time("::").has_value());
}

TEST(FormatHhmm, PrintsPaddedClockTime) {
    EXPECT_EQ(limestone::format_hhmm(30600), "08:30");
    EXPECT_EQ(limestone::format_hhmm(90600), "25:10");
}