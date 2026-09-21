#include "limestone/csv.hpp"

#include <stdexcept>

namespace limestone {
namespace {

const std::string kEmptyField;

void strip_line_ending(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

void strip_byte_order_mark(std::string& line) {
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
}

}

std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (quoted) {
            if (c != '"') {
                field.push_back(c);
            } else if (i + 1 < line.size() && line[i + 1] == '"') {
                field.push_back('"');
                ++i;
            } else {
                quoted = false;
            }
            continue;
        }

        if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(c);
        }
    }

    fields.push_back(field);
    return fields;
}

CsvReader::CsvReader(const std::string& path) : path_(path), in_(path) {
    if (!in_) {
        throw std::runtime_error("cannot open " + path);
    }

    std::string header;
    if (!std::getline(in_, header)) {
        throw std::runtime_error("empty file " + path);
    }
    ++line_number_;

    strip_byte_order_mark(header);
    strip_line_ending(header);

    const std::vector<std::string> names = parse_csv_line(header);
    for (std::size_t i = 0; i < names.size(); ++i) {
        columns_.emplace(names[i], i);
    }
}

bool CsvReader::next() {
    std::string line;
    while (std::getline(in_, line)) {
        ++line_number_;
        strip_line_ending(line);
        if (line.empty()) {
            continue;
        }
        fields_ = parse_csv_line(line);
        return true;
    }
    return false;
}

bool CsvReader::has_column(const std::string& column) const {
    return columns_.find(column) != columns_.end();
}

// Missing columns and short rows read as empty rather than throwing, since
// GTFS marks many columns optional and feeds truncate trailing empties.
const std::string& CsvReader::get(const std::string& column) const {
    const auto it = columns_.find(column);
    if (it == columns_.end() || it->second >= fields_.size()) {
        return kEmptyField;
    }
    return fields_[it->second];
}

}