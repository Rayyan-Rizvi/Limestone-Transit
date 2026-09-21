#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace limestone {

// Maps GTFS string ids to dense integer indices so the routing code can work
// in integers and index directly into the feed's vectors.
class Interner {
public:
    static constexpr int kMissing = -1;

    int intern(const std::string& id);
    int lookup(const std::string& id) const;

    const std::string& name(int index) const { return names_[index]; }
    std::size_t size() const { return names_.size(); }

private:
    std::unordered_map<std::string, int> indices_;
    std::vector<std::string> names_;
};

}