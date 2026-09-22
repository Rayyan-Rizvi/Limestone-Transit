#pragma once

#include <string>
#include <vector>

namespace limestone {

// A stop to plot. minutes below zero means it was never reachable.
struct MapPoint {
    double lat = 0.0;
    double lon = 0.0;
    int minutes = -1;
};

struct MapStyle {
    int width = 1000;
    int margin = 46;
    double radius = 3.4;
};

const char* colour_for_minutes(int minutes);

void write_svg_map(const std::string& path, const std::vector<MapPoint>& points, double origin_lat,
                   double origin_lon, const std::string& title, const std::string& subtitle,
                   const MapStyle& style = MapStyle{});

}