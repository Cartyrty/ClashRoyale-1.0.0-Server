#pragma once
// Alliance roles (alliance_roles.csv) and what each may do, as the client decides it
// (AllianceMemberEntry::hasLowerRole, ChangeAllianceMemberRoleMessage::hasRights).
namespace server {
namespace AllianceRole {
constexpr int kMember = 1;
constexpr int kLeader = 2;
constexpr int kElder = 3;
constexpr int kCoLeader = 4;

constexpr bool isValid(int role) { return role >= kMember && role <= kCoLeader; }

// Who takes over when the leader leaves: the highest authority.
constexpr int getAuthority(int role) { return role == kLeader ? 20 : role == kCoLeader ? 10 : role == kElder ? 5 : 1; }

constexpr bool canKick(int role) { return role != kMember; }
constexpr bool canAcceptJoinRequests(int role) { return role != kMember; }
constexpr bool canChangeSettings(int role) { return role == kLeader || role == kCoLeader; }
constexpr bool canSendMail(int role) { return role == kLeader || role == kCoLeader; }

constexpr bool hasLowerRole(int role, int than) {
    if (than == kCoLeader) return role != kCoLeader && role != kLeader;
    if (than == kElder) return role == kMember;
    if (than == kLeader) return role != kLeader;
    return false;
}

constexpr bool hasRights(int actorRole, int targetRole, int newRole) {
    if (!hasLowerRole(targetRole, actorRole)) return false;
    if (actorRole == newRole && (actorRole == kLeader || actorRole == kCoLeader)) return true;
    return hasLowerRole(newRole, actorRole);
}
} // namespace AllianceRole
} // namespace server
