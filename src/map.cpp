#include "limestone/map.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace limestone {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Bounds {
    double min_lat = 0.0;
    double max_lat = 0.0;
    double min_lon = 0.0;
    double max_lon = 0.0;
};

Bounds bounds_of(const std::vector<MapPoint>& points) {
    Bounds bounds{points.front().lat, points.front().lat, points.front().lon, points.front().lon};
    for (const MapPoint& point : points) {
        bounds.min_lat = std::min(bounds.min_lat, point.lat);
        bounds.max_lat = std::max(bounds.max_lat, point.lat);
        bounds.min_lon = std::min(bounds.min_lon, point.lon);
        bounds.max_lon = std::max(bounds.max_lon, point.lon);
    }
    return bounds;
}

std::string escape(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c == '&') {
            out += "&amp;";
        } else if (c == '<') {
            out += "&lt;";
        } else if (c == '>') {
            out += "&gt;";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

}

const char* colour_for_minutes(int minutes) {
    if (minutes < 0) {
        return "#d9d9d9";
    }
    if (minutes <= 15) {
        return "#1a9850";
    }
    if (minutes <= 30) {
        return "#a6d96a";
    }
    if (minutes <= 45) {
        return "#fdae61";
    }
    if (minutes <= 60) {
        return "#f46d43";
    }
    return "#a50026";
}

// Equirectangular projection: good enough over one city, and longitude is
// scaled by cos(latitude) so the shape is not stretched sideways.
void write_svg_map(const std::string& path, const std::vector<MapPoint>& points, double origin_lat,
                   double origin_lon, const std::string& title, const std::string& subtitle,
                   const MapStyle& style) {
    if (points.empty()) {
        throw std::runtime_error("no points to plot");
    }

    const Bounds bounds = bounds_of(points);
    const double mid_lat = (bounds.min_lat + bounds.max_lat) / 2.0;
    const double lon_scale = std::cos(mid_lat * kPi / 180.0);

    const double span_x = std::max(1e-9, (bounds.max_lon - bounds.min_lon) * lon_scale);
    const double span_y = std::max(1e-9, bounds.max_lat - bounds.min_lat);

    const int header = 78;
    const int legend = 54;
    const double plot_width = style.width - 2.0 * style.margin;
    const double plot_height = plot_width * span_y / span_x;
    const int height = static_cast<int>(plot_height) + header + legend;
    const auto to_x = [&](double lon) {
        return style.margin + (lon - bounds.min_lon) * lon_scale / span_x * plot_width;
    };
    const auto to_y = [&](double lat) {
        return header + (bounds.max_lat - lat) / span_y * plot_height;
    };

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write " + path);
    }

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << style.width << "\" height=\""
        << height << "\" viewBox=\"0 0 " << style.width << ' ' << height << "\">\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    out << "<text x=\"" << style.margin << "\" y=\"38\" font-family=\"Helvetica, Arial, sans-serif\""
        << " font-size=\"22\" font-weight=\"600\" fill=\"#111111\">" << escape(title) << "</text>\n";
    out << "<text x=\"" << style.margin << "\" y=\"62\" font-family=\"Helvetica, Arial, sans-serif\""
        << " font-size=\"14\" fill=\"#555555\">" << escape(subtitle) << "</text>\n";

    // Slow and unreachable stops go down first so the fast ones stay visible
    // where dots overlap downtown.
    std::vector<const MapPoint*> ordered;
    ordered.reserve(points.size());
    for (const MapPoint& point : points) {
        ordered.push_back(&point);
    }
    std::sort(ordered.begin(), ordered.end(), [](const MapPoint* a, const MapPoint* b) {
        const int left = a->minutes < 0 ? 100000 : a->minutes;
        const int right = b->minutes < 0 ? 100000 : b->minutes;
        return left > right;
    });

    for (const MapPoint* point : ordered) {
        out << "<circle cx=\"" << to_x(point->lon) << "\" cy=\"" << to_y(point->lat) << "\" r=\""
            << style.radius << "\" fill=\"" << colour_for_minutes(point->minutes)
            << "\" fill-opacity=\"0.9\"/>\n";
    }

    out << "<circle cx=\"" << to_x(origin_lon) << "\" cy=\"" << to_y(origin_lat)
        << "\" r=\"9\" fill=\"none\" stroke=\"#111111\" stroke-width=\"2.5\"/>\n";
    out << "<circle cx=\"" << to_x(origin_lon) << "\" cy=\"" << to_y(origin_lat)
        << "\" r=\"2.5\" fill=\"#111111\"/>\n";

    const char* labels[] = {"to 15 min", "to 30 min", "to 45 min", "to 60 min", "over 60 min",
                            "no service"};
    const int samples[] = {10, 25, 40, 55, 90, -1};
    double x = style.margin;
    const double y = height - 18;

    for (int i = 0; i != 6; ++i) {
        out << "<circle cx=\"" << x + 6 << "\" cy=\"" << y - 5 << "\" r=\"6\" fill=\""
            << colour_for_minutes(samples[i]) << "\"/>\n";
        out << "<text x=\"" << x + 18 << "\" y=\"" << y
            << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"13\" fill=\"#333333\">"
            << labels[i] << "</text>\n";
        x += 148;
    }

    out << "</svg>\n";
}

}