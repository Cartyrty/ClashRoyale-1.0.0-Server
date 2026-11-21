#pragma once
// A leaderboard row, as RankingEntry is on the client. Sent as the library's own AvatarRankingEntry or
// AllianceRankingEntry, each subclass making its object and this filling the RankingEntry part.
#include "library/Library.h"

#include <cstdint>
#include <string>

namespace server {
class RankingEntry {
public:
    virtual ~RankingEntry() = default;

    // A new library ranking entry; its owner (a ranking list message's list) frees it.
    virtual library::Ptr createLibraryEntry() const = 0;

    void setId(std::int64_t id) { id_ = id; }
    void setName(const std::string &name) { name_ = name; }
    void setOrder(int order) {
        order_ = order;
        previousOrder_ = order; // the server keeps no history
    }
    void setScore(int score) { score_ = score; }

protected:
    // RankingEntry: id LogicLong* +0x08, name String* +0x10, order +0x18, score +0x1c, previous order +0x20.
    void fillRankingEntry(library::Ptr entry) const {
        constexpr int kIdOffset = 0x08;
        constexpr int kNameOffset = 0x10;
        constexpr int kOrderOffset = 0x18;
        constexpr int kScoreOffset = 0x1c;
        constexpr int kPreviousOrderOffset = 0x20;
        library::writePointer(entry + kIdOffset, library::newLogicLong(id_));
        library::writePointer(entry + kNameOffset, library::newString(name_));
        library::writeInt(entry + kOrderOffset, order_);
        library::writeInt(entry + kScoreOffset, score_);
        library::writeInt(entry + kPreviousOrderOffset, previousOrder_);
    }

private:
    std::int64_t id_ = 0;
    std::string name_;
    int order_ = 0;
    int score_ = 0;
    int previousOrder_ = 0;
};
} // namespace server
