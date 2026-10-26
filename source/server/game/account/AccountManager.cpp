#include "game/account/AccountManager.h"

#include "game/database/Database.h"
#include "game/utils/FileUtil.h"
#include "game/utils/TimeUtil.h"
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/data/LogicData.h"
#include "logic/data/LogicDataTables.h"
#include "logic/home/LogicClientHome.h"
#include "logic/home/LogicSpellDeck.h"
#include "logic/message/home/OwnHomeDataMessage.h"
#include "logic/mode/LogicGameMode.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <random>

namespace server {
namespace fs = std::filesystem;

namespace {
constexpr int kPassTokenLength = 40;
constexpr const char *kPassTokenAlphabet = "abcdefghijklmnopqrstuvwxyz0123456789";

// The new account: Training Camp's final deck, at level 1, in Arena 1.
constexpr const char *kStartingDeck = "BasicDeck8";
constexpr const char *kStartingArena = "Arena1";
constexpr const char *kDefaultName = "Player";
constexpr int kDeckSize = 8;
constexpr int kGoldCommodity = 0; // LogicClientAvatar commodity type of resources (Gold, FreeGold)

// The Node v1 server's account keeps its id and pass token.
constexpr std::int64_t kNodeAccountId = 1;
constexpr const char *kNodeAccountPassToken = "nypbej3nc7cbcz3bk2mcxtx2x6bazd6xnt7ec7xs";
constexpr std::int64_t kNodeAccountCreatedMs = 1620660618000;

// Account files: "CRA1", then little-endian fields and length-prefixed blobs.
constexpr const char *kAccountFileMagic = "CRA1";
constexpr std::size_t kAccountFileMagicSize = 4;

class AccountFileReader {
public:
    explicit AccountFileReader(const std::string &data) : data_(data), offset_(kAccountFileMagicSize) {}

    template <class T> T read() {
        T value{};
        if (offset_ + sizeof(T) > data_.size()) {
            ok_ = false;
            return value;
        }
        std::memcpy(&value, data_.data() + offset_, sizeof(T));
        offset_ += sizeof(T);
        return value;
    }
    std::string readBlob() {
        const auto length = read<std::uint32_t>();
        if (!ok_ || offset_ + length > data_.size()) {
            ok_ = false;
            return {};
        }
        auto blob = data_.substr(offset_, length);
        offset_ += length;
        return blob;
    }
    bool isOk() const { return ok_; }

private:
    const std::string &data_;
    std::size_t offset_;
    bool ok_ = true;
};

std::vector<std::uint8_t> toBytes(const std::string &text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::string createPassToken() {
    static std::mt19937_64 random{std::random_device{}()};
    const std::size_t alphabetSize = std::strlen(kPassTokenAlphabet);
    std::string token;
    for (int i = 0; i < kPassTokenLength; ++i) token += kPassTokenAlphabet[random() % alphabetSize];
    return token;
}
} // namespace

AccountManager::AccountManager(Database &database, std::string dataDirectory)
    : database_(database), dataDirectory_(std::move(dataDirectory)) {
    loadAccounts();
    if (accounts_.empty()) importAccountFiles();
    if (accounts_.empty()) importNodeAccount();
}

void AccountManager::loadAccounts() {
    auto query = database_.prepare(
        "SELECT id, pass_token, home, avatar, saved_at, created_ms, session_count, play_seconds FROM accounts");
    while (query.step()) {
        Account account;
        account.id = query.integer(0);
        account.passToken = query.text(1);
        account.home = query.blob(2);
        account.avatar = query.blob(3);
        account.savedAt = query.integer(4);
        account.createdMs = query.integer(5);
        account.sessionCount = int(query.integer(6));
        account.playSeconds = int(query.integer(7));
        nextAccountId_ = std::max(nextAccountId_, account.id + 1);
        accounts_[account.id] = std::move(account);
    }
    Debugger::print("server", "%zu accounts", accounts_.size());
}

// The folder is left in place.
void AccountManager::importAccountFiles() {
    const auto folder = fs::path(dataDirectory_) / "accounts";
    if (!fs::is_directory(folder)) return;
    Database::Transaction transaction(database_);
    for (const auto &file : fs::directory_iterator(folder)) {
        if (file.path().extension() != ".account") continue;
        const auto data = FileUtil::readFile(file.path());
        if (data.compare(0, kAccountFileMagicSize, kAccountFileMagic) != 0) continue;
        AccountFileReader reader(data);
        Account account;
        account.id = reader.read<std::int64_t>();
        account.savedAt = reader.read<std::int64_t>();
        account.createdMs = reader.read<std::int64_t>();
        account.sessionCount = reader.read<std::int32_t>();
        account.playSeconds = reader.read<std::int32_t>();
        account.passToken = reader.readBlob();
        account.home = toBytes(reader.readBlob());
        account.avatar = toBytes(reader.readBlob());
        if (!reader.isOk()) {
            Debugger::warning("account file %s is damaged", file.path().string().c_str());
            continue;
        }
        nextAccountId_ = std::max(nextAccountId_, account.id + 1);
        saveAccount(account);
        accounts_[account.id] = std::move(account);
    }
    transaction.commit();
    if (!accounts_.empty()) Debugger::print("server", "imported %zu account files into server.db", accounts_.size());
}

void AccountManager::importNodeAccount() {
    const auto file = fs::path(dataDirectory_) / "import" / "account-1.ownhome";
    if (!fs::exists(file)) return;
    Account account;
    account.id = kNodeAccountId;
    account.passToken = kNodeAccountPassToken;
    account.createdMs = kNodeAccountCreatedMs;
    if (!OwnHomeDataMessage::decodeHomeAndAvatar(FileUtil::readBytes(file), account.home, account.avatar)) {
        Debugger::warning("the Node account export could not be decoded");
        return;
    }
    account.savedAt = TimeUtil::getSeconds();
    accounts_[account.id] = account;
    nextAccountId_ = std::max(nextAccountId_, kNodeAccountId + 1);
    saveAccount(account);
    Debugger::print("server", "imported the Node v1 account (id 1)");
}

std::vector<std::int64_t> AccountManager::getAccountIds() const {
    std::vector<std::int64_t> ids;
    for (const auto &[id, account] : accounts_) ids.push_back(id);
    return ids;
}

Account *AccountManager::getAccount(std::int64_t id) {
    const auto found = accounts_.find(id);
    return found == accounts_.end() ? nullptr : &found->second;
}

// A new player with the library's defaults: level 1, no experience or trophies, globals.csv STARTING_GOLD /
// STARTING_DIAMONDS, the Training Camp's final deck and Arena 1. The home and avatar start from the new-account
// template (its avatar carries the achievement and data-slot lists a constructed LogicClientAvatar lacks) and every
// value is reset; the tutorials are skipped with the library's own LogicClientHome::debugFinishTutorial (every home
// tutorial done, the Training Camp NPC wins credited).
Account &AccountManager::createAccount() {
    Account account;
    account.id = nextAccountId_++;
    account.passToken = createPassToken();
    account.createdMs = TimeUtil::getMilliseconds();
    account.savedAt = TimeUtil::getSeconds();
    std::vector<std::uint8_t> templateHome;
    std::vector<std::uint8_t> templateAvatar;
    const auto templateFile = fs::path(dataDirectory_) / "import" / "new-account.ownhome";
    if (!fs::exists(templateFile) ||
        !OwnHomeDataMessage::decodeHomeAndAvatar(FileUtil::readBytes(templateFile), templateHome, templateAvatar))
        Debugger::error("new-account template missing: run tools/export_node_state.js (see README)");

    auto home = LogicClientHome::decode(templateHome);
    home.setHomeId(account.id);
    if (home.getDeckCount() == 0) home.addEmptyDeck(); // getSelectedDeck does not check
    const auto deck = home.getSelectedDeck();
    LogicSpellDeck::clear(deck);
    const auto spells = LogicData::getPredefinedDeckSpells(LogicDataTables::getPredefinedDeckByName(kStartingDeck));
    for (int slot = 0; slot < library::arrayCount(spells) && slot < kDeckSize; ++slot)
        LogicSpellDeck::putSpell(deck, slot, library::arrayItem(spells, slot), 0);
    home.destroyChests();

    auto avatar = LogicClientAvatar::decode(templateAvatar);
    avatar.setIds(account.id);
    avatar.setName(kDefaultName);
    avatar.setExpLevel(1);
    avatar.setExpPoints(0);
    avatar.setScore(0);
    const int startingGold = LogicDataTables::getIntGlobal("STARTING_GOLD");
    const int startingDiamonds = LogicDataTables::getIntGlobal("STARTING_DIAMONDS");
    avatar.setCommodityCount(kGoldCommodity, LogicDataTables::getGoldData(), startingGold);
    avatar.setCommodityCount(kGoldCommodity, LogicDataTables::getFreeGoldData(), startingGold);
    avatar.setDiamonds(startingDiamonds);
    avatar.setFreeDiamonds(startingDiamonds);

    // debugFinishTutorial reaches the avatar through the home's game mode.
    LogicGameMode mode(false);
    mode.loadHomeState(std::move(home), std::move(avatar), 0, 0);
    mode.getHome().debugFinishTutorial();
    if (const auto arena = LogicDataTables::getArenaByName(kStartingArena)) mode.getPlayerAvatar().setArena(arena);
    account.home = mode.getHome().encode();
    account.avatar = mode.getPlayerAvatar().encode();

    auto &stored = accounts_[account.id] = std::move(account);
    saveAccount(stored);
    Debugger::print("server", "created account %lld", (long long)stored.id);
    return stored;
}

void AccountManager::saveAccount(const Account &account) {
    database_
        .prepare(
            "INSERT INTO accounts (id, pass_token, home, avatar, saved_at, created_ms, session_count, play_seconds) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET pass_token = excluded.pass_token, "
            "home = excluded.home, avatar = excluded.avatar, saved_at = excluded.saved_at, created_ms = "
            "excluded.created_ms, "
            "session_count = excluded.session_count, play_seconds = excluded.play_seconds")
        .bind(1, account.id)
        .bind(2, account.passToken)
        .bindBlob(3, account.home)
        .bindBlob(4, account.avatar)
        .bind(5, account.savedAt)
        .bind(6, account.createdMs)
        .bind(7, account.sessionCount)
        .bind(8, account.playSeconds)
        .run();
}
} // namespace server
