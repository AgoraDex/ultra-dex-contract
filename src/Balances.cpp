#include <Contract.hpp>

using namespace std;
using namespace eosio;

void Contract::SubBalance(const name user, const asset value) {
    BalancesTable balances {get_self(), user.value};

    const auto balance_it = balances.require_find(value.symbol.code().raw(), "no balance object found");
    check(balance_it->balance.amount >= value.amount, "overdrawn balance");

    balances.modify(balance_it, user, [&](BalanceRecord& record ) {
        record.balance -= value;
    });
}

void Contract::AddBalance(const name user, const asset value) {
    BalancesTable balances {get_self(), user.value};

    const auto balance_it = balances.find(value.symbol.code().raw());

    if (balance_it == balances.end()) {
        balances.emplace(get_self(), [&](BalanceRecord& record){
            record.balance = value;
        });
    } else {
        balances.modify(balance_it, same_payer, [&](BalanceRecord& record) {
            record.balance += value;
        });
    }
}
