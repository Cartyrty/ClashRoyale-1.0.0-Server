#include "game/database/Database.h"
#include <sqlite3.h>
#include "titan/logic/debug/Debugger.h"

namespace server {
namespace {
// Schema version 1. Accounts keep the library's own encodings (LogicClientHome / LogicClientAvatar::encode); clan
// stream entries and the avatar's social state stay JSON documents (their fields vary by entry type).
const char *kSchema = R"sql(
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS accounts (
    id            INTEGER PRIMARY KEY,
    pass_token    TEXT    NOT NULL,
    home          BLOB    NOT NULL,
    avatar        BLOB    NOT NULL,
    saved_at      INTEGER NOT NULL DEFAULT 0,
    created_ms    INTEGER NOT NULL DEFAULT 0,
    session_count INTEGER NOT NULL DEFAULT 0,
    play_seconds  INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS clans (
    id             INTEGER PRIMARY KEY,
    name           TEXT    NOT NULL,
    description    TEXT    NOT NULL DEFAULT '',
    badge          INTEGER NOT NULL DEFAULT 0,
    type           INTEGER NOT NULL DEFAULT 1,
    required_score INTEGER NOT NULL DEFAULT 0,
    region         INTEGER NOT NULL DEFAULT 0,
    next_entry     INTEGER NOT NULL DEFAULT 1
);
CREATE TABLE IF NOT EXISTS clan_members (
    account_id INTEGER PRIMARY KEY,
    clan_id    INTEGER NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    name       TEXT    NOT NULL,
    role       INTEGER NOT NULL,
    position   INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS clan_members_clan ON clan_members(clan_id, position);
CREATE TABLE IF NOT EXISTS clan_entries (
    clan_id  INTEGER NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    position INTEGER NOT NULL,
    data     TEXT    NOT NULL,
    PRIMARY KEY (clan_id, position)
);
CREATE TABLE IF NOT EXISTS avatar_social (
    account_id INTEGER PRIMARY KEY,
    data       TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS replays (
    id         INTEGER PRIMARY KEY,
    data       TEXT    NOT NULL,
    created_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS battle_reports (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    replay_id  INTEGER NOT NULL,
    time_ms    INTEGER NOT NULL,
    log        TEXT    NOT NULL,
    arena      INTEGER NOT NULL,
    sender     INTEGER NOT NULL,
    name       TEXT    NOT NULL,
    level      INTEGER NOT NULL,
    is_new     INTEGER NOT NULL DEFAULT 1
);
CREATE INDEX IF NOT EXISTS battle_reports_account ON battle_reports(account_id, id);
)sql";
} // namespace

Database::Database(const std::string &path) : path_(path) {
    if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK)
        Debugger::error("cannot open the database %s: %s", path.c_str(), db_ ? sqlite3_errmsg(db_) : "out of memory");
    sqlite3_busy_timeout(db_, 5000);
    exec("PRAGMA journal_mode = WAL");
    exec("PRAGMA synchronous = NORMAL");
    exec("PRAGMA foreign_keys = ON");
    exec(kSchema);
    if (!meta("schema_version")) setMeta("schema_version", 1);
}
Database::~Database() { sqlite3_close_v2(db_); }

void Database::exec(const char *sql) {
    char *error = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        Debugger::error("database error: %s", message.c_str());
    }
}
std::int64_t Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }
std::int64_t Database::count(const char *table) {
    Statement s(db_, (std::string("SELECT COUNT(*) FROM ") + table).c_str());
    return s.step() ? s.integer(0) : 0;
}
std::optional<std::int64_t> Database::meta(const std::string &key) {
    Statement s(db_, "SELECT value FROM meta WHERE key = ?");
    s.bind(1, key);
    if (!s.step()) return std::nullopt;
    return s.integer(0);
}
void Database::setMeta(const std::string &key, std::int64_t value) {
    Statement s(db_,
                "INSERT INTO meta (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    s.bind(1, key).bind(2, value).run();
}

Database::Statement::Statement(sqlite3 *db, const char *sql) : db_(db) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK)
        Debugger::error("database error: %s in %s", sqlite3_errmsg(db), sql);
}
Database::Statement::~Statement() { sqlite3_finalize(stmt_); }
Database::Statement &Database::Statement::bind(int index, std::int64_t value) {
    sqlite3_bind_int64(stmt_, index, value);
    return *this;
}
Database::Statement &Database::Statement::bind(int index, const std::string &text) {
    sqlite3_bind_text(stmt_, index, text.data(), int(text.size()), SQLITE_TRANSIENT);
    return *this;
}
Database::Statement &Database::Statement::bindBlob(int index, const std::vector<std::uint8_t> &bytes) {
    sqlite3_bind_blob(stmt_, index, bytes.data(), int(bytes.size()), SQLITE_TRANSIENT);
    return *this;
}
bool Database::Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc != SQLITE_DONE) Debugger::error("database error: %s", sqlite3_errmsg(db_));
    return false;
}
void Database::Statement::run() {
    while (step()) {}
    sqlite3_reset(stmt_); // ready to be bound and run again
    sqlite3_clear_bindings(stmt_);
}
std::int64_t Database::Statement::integer(int column) const { return sqlite3_column_int64(stmt_, column); }
std::string Database::Statement::text(int column) const {
    const auto *p = reinterpret_cast<const char *>(sqlite3_column_text(stmt_, column));
    return p ? std::string(p, std::size_t(sqlite3_column_bytes(stmt_, column))) : std::string();
}
std::vector<std::uint8_t> Database::Statement::blob(int column) const {
    const auto *p = static_cast<const std::uint8_t *>(sqlite3_column_blob(stmt_, column));
    const int n = sqlite3_column_bytes(stmt_, column);
    return p && n > 0 ? std::vector<std::uint8_t>(p, p + n) : std::vector<std::uint8_t>();
}

Database::Transaction::Transaction(Database &db) : db_(db) { db_.exec("BEGIN IMMEDIATE"); }
Database::Transaction::~Transaction() {
    if (!done_) sqlite3_exec(db_.db_, "ROLLBACK", nullptr, nullptr, nullptr);
}
void Database::Transaction::commit() {
    db_.exec("COMMIT");
    done_ = true;
}
} // namespace server
