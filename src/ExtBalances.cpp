#include <Contract.hpp>
#include <eosio.token.hpp>

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

    ExtendedBalancesTable balances_table { get_self(), from.value };
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

void Contract::AddExtBalance(const name user, const extended_asset to_add) {
    check(to_add.quantity.is_valid(), "invalid asset");

    ExtendedBalancesTable balances {get_self(), user.value};
    auto index = balances.get_index<"extended"_n>();

    const auto balance_it = index.find(GetIndexFromToken(to_add.get_extended_symbol()));

    if (balance_it == index.end()) {
        check(to_add.quantity.amount > 0, "Insufficient funds");

        balances.emplace(get_self(), [&](ExtendedBalanceRecord& record) {
            record.id = balances.available_primary_key();
            record.balance = to_add;
        });
    } else {
        index.modify(balance_it, get_self(), [&](ExtendedBalanceRecord& record) {
            check(to_add.quantity.amount + record.balance.quantity.amount >= 0,
                  "Insufficient funds, you have " + record.balance.quantity.to_string()
                  + ", but need " + (to_add.quantity * -1).to_string());

            record.balance += to_add;
        });
    }
}

void Contract::SubExtBalance(const name user, const extended_asset value) {
    AddExtBalance(user, -value);
}

void Contract::Withdraw(eosio::name user, eosio::name withdraw_to, eosio::extended_symbol token) {
    require_auth(user);

    ExtendedBalancesTable balances_table { get_self(), user.value };
    auto index = balances_table.get_index<"extended"_n>();

    const auto balance_it = index.find(GetIndexFromToken(token));

    check(balance_it != index.end(), "Token not found");

    const extended_asset to_transfer = balance_it->balance;
    index.erase(balance_it);

    if (to_transfer.quantity.amount < 1) {
        return;
    }

    token::transfer_action transfer_action(to_transfer.contract, {get_self(), "active"_n});
    transfer_action.send(get_self(), withdraw_to, to_transfer.quantity, "withdraw");
}
