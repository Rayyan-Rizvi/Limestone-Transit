#include "limestone/interner.hpp"

namespace limestone {

int Interner::intern(const std::string& id) {
    const auto it = indices_.find(id);
    if (it != indices_.end()) {
        return it->second;
    }

    const int index = static_cast<int>(names_.size());
    names_.push_back(id);
    indices_.emplace(id, index);
    return index;
}

int Interner::lookup(const std::string& id) const {
    const auto it = indices_.find(id);
    return it == indices_.end() ? kMissing : it->second;
}

}