#include <Contract.hpp>

using namespace std;
using namespace eosio;

void Contract::SubBalance(const name user, const asset value) {
    BalancesTable balances { get_self(), user.value };

    const auto balance_it = balances.require_find(value.symbol.code().raw(), "user balance not found");
    check(balance_it->balance.amount >= value.amount, "overdrawn balance");

    if (balance_it->balance.amount == value.amount) {
        balances.erase(balance_it);
        return;
    }

    balances.modify(balance_it, get_self(), [&](BalanceRecord& record) {
        record.balance -= value;
    });
}

void Contract::AddBalance(const name user, const asset value) {
    BalancesTable balances { get_self(), user.value };

    const auto balance_it = balances.find(value.symbol.code().raw());

    if (balance_it == balances.end()) {
        balances.emplace(get_self(), [&](BalanceRecord& record){
            record.balance = value;
        });
    } else {
        balances.modify(balance_it, get_self(), [&](BalanceRecord& record) {
            record.balance += value;
        });
    }
}

void Contract::Transfer(name from, name to, asset quantity, const string& memo) {
    check(from != to, "cannot transfer to self");
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(memo.size() <= 256, "memo has more than 256 bytes");
    check(is_account(to), "to account does not exist");

    require_auth(from);

    require_recipient(from);
    require_recipient(to);

    SubBalance(from, quantity);
    AddBalance(to, quantity);
}
