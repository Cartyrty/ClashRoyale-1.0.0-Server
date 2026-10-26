#include "logic/message/LogicMessageFactory.h"

#include "logic/message/account/ClientCapabilitiesMessage.h"
#include "logic/message/account/KeepAliveMessage.h"
#include "logic/message/account/LoginMessage.h"
#include "logic/message/account/SetDeviceTokenMessage.h"
#include "logic/message/alliance/AskForAllianceDataMessage.h"
#include "logic/message/alliance/AskForAllianceStreamMessage.h"
#include "logic/message/alliance/AskForJoinableAlliancesListMessage.h"
#include "logic/message/alliance/ChangeAllianceMemberRoleMessage.h"
#include "logic/message/alliance/ChangeAllianceSettingsMessage.h"
#include "logic/message/alliance/ChatToAllianceStreamMessage.h"
#include "logic/message/alliance/CreateAllianceMessage.h"
#include "logic/message/alliance/DonateAllianceUnitMessage.h"
#include "logic/message/alliance/JoinAllianceMessage.h"
#include "logic/message/alliance/LeaveAllianceMessage.h"
#include "logic/message/alliance/RequestJoinAllianceMessage.h"
#include "logic/message/alliance/RespondToAllianceJoinRequestMessage.h"
#include "logic/message/alliance/SearchAlliancesMessage.h"
#include "logic/message/avatar/AcceptChallengeMessage.h"
#include "logic/message/avatar/AskForAvatarStreamMessage.h"
#include "logic/message/avatar/AskForBattleReplayStreamMessage.h"
#include "logic/message/avatar/AvatarNameCheckRequestMessage.h"
#include "logic/message/avatar/CancelChallengeMessage.h"
#include "logic/message/avatar/ChangeAvatarNameMessage.h"
#include "logic/message/battle/SendBattleEventMessage.h"
#include "logic/message/friend/AskForPlayingFacebookFriendsMessage.h"
#include "logic/message/friend/AskForPlayingGamecenterFriendsMessage.h"
#include "logic/message/home/CancelMatchmakeMessage.h"
#include "logic/message/home/EndClientTurnMessage.h"
#include "logic/message/home/GoHomeMessage.h"
#include "logic/message/home/HomeBattleReplayMessage.h"
#include "logic/message/home/HomeBattleReplayViewedMessage.h"
#include "logic/message/home/HomeLogicStoppedMessage.h"
#include "logic/message/home/StartMissionMessage.h"
#include "logic/message/home/VisitHomeMessage.h"
#include "logic/message/ranking/AskForAllianceRankingListMessage.h"
#include "logic/message/ranking/AskForAvatarLocalRankingListMessage.h"
#include "logic/message/ranking/AskForAvatarRankingListMessage.h"
#include "logic/message/royaltv/AskForTVContentMessage.h"
#include "logic/message/sector/RequestSectorStateMessage.h"
#include "logic/message/sector/SectorCommandMessage.h"
#include "logic/message/udp/UdpCheckConnectionMessage.h"
#include "titan/logic/message/UnknownMessage.h"

namespace server {
std::unique_ptr<PiranhaMessage> LogicMessageFactory::createMessageByType(int messageType) const {
    if (udp_) {
        switch (messageType) {
        case UdpCheckConnectionMessage::kMessageType: return std::make_unique<UdpCheckConnectionMessage>();
        case RequestSectorStateMessage::kMessageType: return std::make_unique<RequestSectorStateMessage>();
        case SectorCommandMessage::kMessageType: return std::make_unique<SectorCommandMessage>();
        }
        return nullptr;
    }
    switch (messageType) {
    case AcceptChallengeMessage::kMessageType: return std::make_unique<AcceptChallengeMessage>();
    case AskForAllianceDataMessage::kMessageType: return std::make_unique<AskForAllianceDataMessage>();
    case AskForAllianceRankingListMessage::kMessageType: return std::make_unique<AskForAllianceRankingListMessage>();
    case AskForAllianceStreamMessage::kMessageType: return std::make_unique<AskForAllianceStreamMessage>();
    case AskForAvatarLocalRankingListMessage::kMessageType:
        return std::make_unique<AskForAvatarLocalRankingListMessage>();
    case AskForAvatarRankingListMessage::kMessageType: return std::make_unique<AskForAvatarRankingListMessage>();
    case AskForAvatarStreamMessage::kMessageType: return std::make_unique<AskForAvatarStreamMessage>();
    case AskForBattleReplayStreamMessage::kMessageType: return std::make_unique<AskForBattleReplayStreamMessage>();
    case AskForJoinableAlliancesListMessage::kMessageType:
        return std::make_unique<AskForJoinableAlliancesListMessage>();
    case AskForPlayingFacebookFriendsMessage::kMessageType:
        return std::make_unique<AskForPlayingFacebookFriendsMessage>();
    case AskForPlayingGamecenterFriendsMessage::kMessageType:
        return std::make_unique<AskForPlayingGamecenterFriendsMessage>();
    case AskForTVContentMessage::kMessageType: return std::make_unique<AskForTVContentMessage>();
    case AvatarNameCheckRequestMessage::kMessageType: return std::make_unique<AvatarNameCheckRequestMessage>();
    case CancelChallengeMessage::kMessageType: return std::make_unique<CancelChallengeMessage>();
    case CancelMatchmakeMessage::kMessageType: return std::make_unique<CancelMatchmakeMessage>();
    case ChangeAllianceMemberRoleMessage::kMessageType: return std::make_unique<ChangeAllianceMemberRoleMessage>();
    case ChangeAllianceSettingsMessage::kMessageType: return std::make_unique<ChangeAllianceSettingsMessage>();
    case ChangeAvatarNameMessage::kMessageType: return std::make_unique<ChangeAvatarNameMessage>();
    case ChatToAllianceStreamMessage::kMessageType: return std::make_unique<ChatToAllianceStreamMessage>();
    case ClientCapabilitiesMessage::kMessageType: return std::make_unique<ClientCapabilitiesMessage>();
    case CreateAllianceMessage::kMessageType: return std::make_unique<CreateAllianceMessage>();
    case DonateAllianceUnitMessage::kMessageType: return std::make_unique<DonateAllianceUnitMessage>();
    case EndClientTurnMessage::kMessageType: return std::make_unique<EndClientTurnMessage>();
    case GoHomeMessage::kMessageType: return std::make_unique<GoHomeMessage>();
    case HomeBattleReplayMessage::kMessageType: return std::make_unique<HomeBattleReplayMessage>();
    case HomeBattleReplayViewedMessage::kMessageType: return std::make_unique<HomeBattleReplayViewedMessage>();
    case HomeLogicStoppedMessage::kMessageType: return std::make_unique<HomeLogicStoppedMessage>();
    case JoinAllianceMessage::kMessageType: return std::make_unique<JoinAllianceMessage>();
    case KeepAliveMessage::kMessageType: return std::make_unique<KeepAliveMessage>();
    case LeaveAllianceMessage::kMessageType: return std::make_unique<LeaveAllianceMessage>();
    case LoginMessage::kMessageType: return std::make_unique<LoginMessage>();
    case RequestJoinAllianceMessage::kMessageType: return std::make_unique<RequestJoinAllianceMessage>();
    case RequestSectorStateMessage::kMessageType: return std::make_unique<RequestSectorStateMessage>();
    case RespondToAllianceJoinRequestMessage::kMessageType:
        return std::make_unique<RespondToAllianceJoinRequestMessage>();
    case SearchAlliancesMessage::kMessageType: return std::make_unique<SearchAlliancesMessage>();
    case SectorCommandMessage::kMessageType: return std::make_unique<SectorCommandMessage>();
    case SendBattleEventMessage::kMessageType: return std::make_unique<SendBattleEventMessage>();
    case SetDeviceTokenMessage::kMessageType: return std::make_unique<SetDeviceTokenMessage>();
    case StartMissionMessage::kMessageType: return std::make_unique<StartMissionMessage>();
    case VisitHomeMessage::kMessageType: return std::make_unique<VisitHomeMessage>();
    }
    return std::make_unique<UnknownMessage>(messageType);
}
} // namespace server
