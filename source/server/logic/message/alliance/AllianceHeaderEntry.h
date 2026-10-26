#pragma once
// A clan as lists show it (search results, joinable clans, the top of a clan's page): Long id, String name, badge,
// ten VInts (type, member count, score, required score, then six unused), locale, region.
#include "titan/logic/datastream/ByteStream.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>
#include <string>

namespace server {
struct AllianceHeaderEntry {
    std::int64_t id = 0;
    std::string name;
    int badge = 0;
    int type = 1;
    int memberCount = 0;
    int score = 0;
    int requiredScore = 0;
    int region = 0;

    void encode(ByteStream &stream) const {
        constexpr int kUnusedValues = 6;
        constexpr int kNoLocale = 0;
        stream.writeLong(id);
        stream.writeStringReference(name);
        ByteStreamHelper::writeDataReference(stream, badge);
        stream.writeVInt(type);
        stream.writeVInt(memberCount);
        stream.writeVInt(score);
        stream.writeVInt(requiredScore);
        for (int i = 0; i < kUnusedValues; ++i) stream.writeVInt(0);
        ByteStreamHelper::writeDataReference(stream, kNoLocale);
        ByteStreamHelper::writeDataReference(stream, region);
    }
};
} // namespace server
