#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "limestone/csv.hpp"

namespace {

std::string write_temp_file(const std::string& name, const std::string& contents) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("limestone_" + name);
    std::ofstream out(path, std::ios::binary);
    out << contents;
    return path.string();
}

}

TEST(ParseCsvLine, SplitsPlainFields) {
    const std::vector<std::string> fields = limestone::parse_csv_line("a,b,c");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[0], "a");
    EXPECT_EQ(fields[2], "c");
}

TEST(ParseCsvLine, KeepsCommasInsideQuotes) {
    const std::vector<std::string> fields =
        limestone::parse_csv_line("S1,\"Princess St, north side\",44.23");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[1], "Princess St, north side");
    EXPECT_EQ(fields[2], "44.23");
}

TEST(ParseCsvLine, UnescapesDoubledQuotes) {
    const std::vector<std::string> fields = limestone::parse_csv_line("\"say \"\"hi\"\"\",b");
    ASSERT_EQ(fields.size(), 2u);
    EXPECT_EQ(fields[0], "say \"hi\"");
}

TEST(ParseCsvLine, PreservesEmptyFields) {
    const std::vector<std::string> fields = limestone::parse_csv_line("a,,c");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[1], "");
}

TEST(CsvReader, ReadsFieldsByColumnName) {
    const std::string path = write_temp_file(
        "stops.txt", "stop_id,stop_name,stop_lat\nS1,Union St,44.22\nS2,Barrie St,44.23\n");

    limestone::CsvReader reader(path);
    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.get("stop_name"), "Union St");
    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.get("stop_id"), "S2");
    EXPECT_FALSE(reader.next());
}

TEST(CsvReader, HandlesByteOrderMarkAndCarriageReturns) {
    const std::string path =
        write_temp_file("bom.txt", "\xEF\xBB\xBFstop_id,stop_name\r\nS1,Union St\r\n");

    limestone::CsvReader reader(path);
    EXPECT_TRUE(reader.has_column("stop_id"));
    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.get("stop_name"), "Union St");
}

TEST(CsvReader, MissingColumnReadsAsEmpty) {
    const std::string path = write_temp_file("min.txt", "stop_id\nS1\n");

    limestone::CsvReader reader(path);
    ASSERT_TRUE(reader.next());
    EXPECT_FALSE(reader.has_column("stop_name"));
    EXPECT_EQ(reader.get("stop_name"), "");
}

TEST(CsvReader, ThrowsOnMissingFile) {
    EXPECT_THROW(limestone::CsvReader("/tmp/limestone_does_not_exist.txt"), std::runtime_error);
}