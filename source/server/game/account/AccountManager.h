#pragma once
// Every account (the accounts table of server.db), loaded at startup. New accounts start as the library makes them:
// see createAccount.
#include "game/account/Account.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace server {
class Database;

class AccountManager {
public:
    AccountManager(Database &database, std::string dataDirectory);

    Account *getAccount(std::int64_t id); // null when unknown
    std::vector<std::int64_t> getAccountIds() const;
    Account &createAccount();
    void saveAccount(const Account &account);

private:
    void loadAccounts();
    void importAccountFiles(); // accounts/*.account from servers before server.db
    void importNodeAccount();  // the Node v1 server's account (tools/export_node_state.js)

    Database &database_;
    std::string dataDirectory_;
    std::map<std::int64_t, Account> accounts_;
    std::int64_t nextAccountId_ = 1;
};
} // namespace server
