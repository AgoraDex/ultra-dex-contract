#include <Contract.hpp>

using namespace std;
using namespace eosio;

void Contract::OnTokenDeposit(name from, name to, asset quantity, const string& memo) {
    if (to != get_self()) {
        return;
    }
    if (from == get_self()) {
        return;
    }
    check(quantity.amount > 0, "Token amount must be positive");

    const extended_asset ext_asset { quantity, get_first_receiver() };
    check(ext_asset.quantity.is_valid(), "invalid asset");

    ExtendedBalancesTable balances_table { get_self(), to.value };
    auto index = balances_table.get_index<"extended"_n>();

    const auto balance_it = index.find(GetIndexFromToken(ext_asset.get_extended_symbol()));

    if (balance_it == index.end()) {
        balances_table.emplace(get_self(), [&](ExtendedBalanceRecord& record) {
            record.id = balances_table.available_primary_key();
            record.balance = ext_asset;
        });
    } else {
        index.modify(balance_it, get_self(), [&](ExtendedBalanceRecord& record) {
            record.balance += ext_asset;
        });
    }
}

void Contract::OnEosTokenDeposit(name from, name to, asset quantity, const string& memo) {
    OnTokenDeposit(from, to, quantity, memo);
}

uint128_t Contract::GetIndexFromToken(eosio::extended_symbol token) {
    return (static_cast<uint128_t>(token.get_contract().value) << 64) ||
           static_cast<uint128_t>(token.get_symbol().raw());
}
