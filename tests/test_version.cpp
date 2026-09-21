#include <gtest/gtest.h>

#include "limestone/version.hpp"

TEST(Version, ReturnsCurrentVersion) {
    EXPECT_EQ(limestone::version(), "0.1.0");
}