#pragma once
// A card request: the requested card, the capacity the request has (the arena's RequestSize), and what each member
// donated so far (a DonationContainer per donor, as on the client).
#include "logic/message/alliance/stream/StreamEntry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class DonateStreamEntry : public StreamEntry {
public:
    struct DonationContainer {
        std::int64_t donorAvatarId = 0;
        std::vector<int> spells; // global ids, one per donated card
    };

    int getStreamEntryType() const override { return kDonate; }

    void setMessage(const std::string &message) { message_ = message; }
    void setRequestedSpells(const std::vector<int> &spells) { requestedSpells_ = spells; }
    bool isRequested(int spellGlobalId) const;
    int getTotalCapacity() const { return totalCapacity_; }
    void setTotalCapacity(int capacity) { totalCapacity_ = capacity; }
    // The capacity donated cards take (each card's rarity DonateCapacity).
    int getUsedCapacity() const;
    int getDonationCount(std::int64_t donorAvatarId) const;
    void addDonation(std::int64_t donorAvatarId, int spellGlobalId);

protected:
    void encodeFields(ByteStream &stream) const override;
    void saveFields(json::Value &saved) const override;
    void loadFields(const json::Value &saved) override;

private:
    std::string message_;
    std::vector<int> requestedSpells_;
    int totalCapacity_ = 0;
    std::vector<DonationContainer> donations_;
};
} // namespace server
