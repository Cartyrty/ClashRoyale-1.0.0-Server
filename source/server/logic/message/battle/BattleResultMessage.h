#pragma once
// A PvP battle's end screen (BattleEndHUD): the outcome, trophies, experience, crowns and chest, and the final state,
// which the client applies with SectorManager::receiveSectorState before the screen. Encoded by the library's own
// BattleResultMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class BattleResultMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20225;
    // BattleEndHUD outcomes.
    static constexpr int kOutcomeWin = 1;
    static constexpr int kOutcomeLoss = 2;
    static constexpr int kOutcomeDraw = 3;

    void encode() override;
    int getMessageType() const override { return kMessageType; }

    void setOutcome(int outcome) { outcome_ = outcome; }
    void setTrophyChange(int trophies) { trophyChange_ = trophies; }
    void setExpReward(int experience) { expReward_ = experience; }
    void setCrowns(int own, int opponent) {
        ownCrowns_ = own;
        opponentCrowns_ = opponent;
    }
    void setChest(int chestGlobalId) { chest_ = chestGlobalId; }
    void setFinalState(const std::vector<std::uint8_t> &state) { finalState_ = state; } // LogicGameMode::encode

private:
    int outcome_ = kOutcomeDraw;
    int trophyChange_ = 0;
    int expReward_ = 0;
    int ownCrowns_ = 0;
    int opponentCrowns_ = 0;
    int chest_ = 0;
    std::vector<std::uint8_t> finalState_;
};
} // namespace server
