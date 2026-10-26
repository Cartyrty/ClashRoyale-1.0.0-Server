#pragma once
// A clan member on the clan's page: Long avatar id, String name, String facebook id, arena, VInt role, VInt exp level,
// VInt score, four VInts, Boolean, Boolean online, VInt, Boolean has home, Long home id.
#include "logic/data/GlobalID.h"
#include "logic/data/LogicDataTables.h"
#include "titan/logic/datastream/ByteStream.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>
#include <string>

namespace server {
struct AllianceMemberEntry {
    std::int64_t avatarId = 0;
    std::string name;
    int role = 1;
    int expLevel = 1;
    int score = 0;
    bool online = false;

    void encode(ByteStream &stream) const {
        constexpr int kUnusedValues = 4;
        const int arena = GlobalID::createGlobalID(LogicDataTables::kArenaClass, 1);
        stream.writeLong(avatarId);
        stream.writeStringReference(name);
        stream.writeStringReference("");
        ByteStreamHelper::writeDataReference(stream, arena);
        stream.writeVInt(role);
        stream.writeVInt(expLevel);
        stream.writeVInt(score);
        for (int i = 0; i < kUnusedValues; ++i) stream.writeVInt(0);
        stream.writeBoolean(false);
        stream.writeBoolean(online);
        stream.writeVInt(0);
        stream.writeBoolean(true);
        stream.writeLong(avatarId);
    }
};
} // namespace server
