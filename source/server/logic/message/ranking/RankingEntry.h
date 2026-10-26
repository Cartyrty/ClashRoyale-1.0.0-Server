#pragma once
// A leaderboard row, as RankingEntry is on the client: LogicLong id, String name, VInt order, VInt score, VInt previous
// order. AvatarRankingEntry and AllianceRankingEntry add their own fields.
#include "titan/logic/datastream/ByteStream.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>
#include <string>

namespace server {
class RankingEntry {
public:
    virtual ~RankingEntry() = default;

    void encode(ByteStream &stream) const {
        ByteStreamHelper::encodeLogicLong(stream, id_);
        stream.writeString(name_);
        stream.writeVInt(order_);
        stream.writeVInt(score_);
        stream.writeVInt(previousOrder_);
        encodeFields(stream);
    }

    void setId(std::int64_t id) { id_ = id; }
    void setName(const std::string &name) { name_ = name; }
    void setOrder(int order) {
        order_ = order;
        previousOrder_ = order; // the server keeps no history
    }
    void setScore(int score) { score_ = score; }

protected:
    virtual void encodeFields(ByteStream &stream) const = 0;

private:
    std::int64_t id_ = 0;
    std::string name_;
    int order_ = 0;
    int score_ = 0;
    int previousOrder_ = 0;
};
} // namespace server
