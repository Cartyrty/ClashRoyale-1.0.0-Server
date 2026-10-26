#pragma once
// The server's storage: one SQLite database (server.db in the data folder) in WAL mode. Prepared statements bind
// values by position; a Transaction groups writes so a save is all-or-nothing.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace server {
class Database {
public:
    explicit Database(const std::string &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    class Statement {
    public:
        Statement(sqlite3 *db, const char *sql);
        ~Statement();
        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;
        Statement &bind(int index, std::int64_t value);
        Statement &bind(int index, int value) { return bind(index, std::int64_t(value)); }
        Statement &bind(int index, const std::string &text);
        Statement &bindBlob(int index, const std::vector<std::uint8_t> &bytes);
        bool step(); // true while a row is available
        void run();  // steps to completion
        std::int64_t integer(int column) const;
        std::string text(int column) const;
        std::vector<std::uint8_t> blob(int column) const;

    private:
        sqlite3 *db_;
        sqlite3_stmt *stmt_ = nullptr;
    };

    class Transaction {
    public:
        explicit Transaction(Database &db);
        ~Transaction(); // rolls back unless committed
        void commit();

    private:
        Database &db_;
        bool done_ = false;
    };

    void exec(const char *sql);
    Statement prepare(const char *sql) { return Statement(db_, sql); }
    std::int64_t lastInsertId() const;
    std::int64_t count(const char *table);
    std::optional<std::int64_t> meta(const std::string &key);
    void setMeta(const std::string &key, std::int64_t value);
    const std::string &path() const { return path_; }

private:
    sqlite3 *db_ = nullptr;
    std::string path_;
};
} // namespace server
