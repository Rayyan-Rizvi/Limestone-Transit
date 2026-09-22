#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "limestone/map.hpp"

namespace {

std::string read_all(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string temp_path(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("limestone_" + name)).string();
}

std::vector<limestone::MapPoint> sample_points() {
    return {{44.2253, -76.4951, 0}, {44.2551, -76.5721, 36}, {44.2647, -76.5489, -1}};
}

}

TEST(MapColours, SeparatesTravelTimeBands) {
    EXPECT_STREQ(limestone::colour_for_minutes(0), limestone::colour_for_minutes(15));
    EXPECT_STRNE(limestone::colour_for_minutes(15), limestone::colour_for_minutes(16));
    EXPECT_STRNE(limestone::colour_for_minutes(60), limestone::colour_for_minutes(61));
}

TEST(MapColours, UsesGreyForUnreachable) {
    EXPECT_STREQ(limestone::colour_for_minutes(-1), "#d9d9d9");
    EXPECT_STRNE(limestone::colour_for_minutes(-1), limestone::colour_for_minutes(90));
}

TEST(WriteSvgMap, PlotsOnePointPerStopPlusOrigin) {
    const std::string path = temp_path("map.svg");
    limestone::write_svg_map(path, sample_points(), 44.2253, -76.4951, "Title", "Subtitle");

    const std::string svg = read_all(path);
    EXPECT_NE(svg.find("<svg"), std::string::npos);
    EXPECT_NE(svg.find("Subtitle"), std::string::npos);

    std::size_t circles = 0;
    for (std::size_t at = svg.find("<circle"); at != std::string::npos;
         at = svg.find("<circle", at + 1)) {
        ++circles;
    }
    // Three stops, two rings for the origin marker, six legend swatches.
    EXPECT_EQ(circles, 11u);
}

TEST(WriteSvgMap, EscapesMarkupInText) {
    const std::string path = temp_path("escape.svg");
    limestone::write_svg_map(path, sample_points(), 44.2253, -76.4951, "A & B", "x < y");

    const std::string svg = read_all(path);
    EXPECT_NE(svg.find("A &amp; B"), std::string::npos);
    EXPECT_NE(svg.find("x &lt; y"), std::string::npos);
}

TEST(WriteSvgMap, RejectsEmptyInput) {
    EXPECT_THROW(limestone::write_svg_map(temp_path("empty.svg"), {}, 0.0, 0.0, "t", "s"),
                 std::runtime_error);
}