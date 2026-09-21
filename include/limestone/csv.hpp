#pragma once

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace limestone {

// Splits one CSV line, honouring double-quoted fields and "" escapes.
std::vector<std::string> parse_csv_line(const std::string& line);

// Reads a GTFS file row by row, addressing fields by column name so that
// column order and optional columns do not matter.
class CsvReader {
public:
    explicit CsvReader(const std::string& path);

    bool next();
    const std::string& get(const std::string& column) const;
    bool has_column(const std::string& column) const;

    std::size_t line_number() const { return line_number_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::ifstream in_;
    std::unordered_map<std::string, std::size_t> columns_;
    std::vector<std::string> fields_;
    std::size_t line_number_ = 0;
};

}