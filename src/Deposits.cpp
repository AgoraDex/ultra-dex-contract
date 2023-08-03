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

    DepositsTable balances_table {get_self(), from.value };
    auto index = balances_table.get_index<"extended"_n>();

    const auto balance_it = index.find(GetIndexFromToken(ext_asset.get_extended_symbol()));

    if (balance_it == index.end()) {
        balances_table.emplace(get_self(), [&](DepositRecord& record) {
            record.id = balances_table.available_primary_key();
            record.balance = ext_asset;
        });
    } else {
        index.modify(balance_it, get_self(), [&](DepositRecord& record) {
            record.balance += ext_asset;
        });
    }
}

void Contract::OnEosTokenDeposit(name from, name to, asset quantity, const string& memo) {
    OnTokenDeposit(from, to, quantity, memo);
}

uint128_t Contract::GetIndexFromToken(const extended_symbol token) {
    return (static_cast<uint128_t>(token.get_contract().value) << 64) + token.get_symbol().raw();
}

void Contract::AddExtBalance(const name user, const extended_asset to_add) {
    check(to_add.quantity.is_valid(), "invalid asset");

    DepositsTable balances {get_self(), user.value };
    auto index = balances.get_index<"extended"_n>();

    const auto balance_it = index.find(GetIndexFromToken(to_add.get_extended_symbol()));

    if (balance_it == index.end()) {
        check(to_add.quantity.amount > 0, "Insufficient funds");

        balances.emplace(get_self(), [&](DepositRecord& record) {
            record.id = balances.available_primary_key();
            record.balance = to_add;
        });
    } else {
        if (balance_it->balance.quantity.amount + to_add.quantity.amount == 0) {
            index.erase(balance_it);
            return;
        }

        index.modify(balance_it, get_self(), [&](DepositRecord& record) {
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

void Contract::Withdraw(const name user, const extended_symbol token) {
    require_auth(user);

    const extended_asset to_transfer = Exchange(user, token);
    check(to_transfer.quantity.amount > 0, "There is nothing to withdraw");

    token::transfer_action transfer_action(to_transfer.contract, { get_self(), "active"_n });
    transfer_action.send(get_self(), user, to_transfer.quantity, "withdraw");
}

extended_asset Contract::Exchange(const name user, const extended_symbol token) {
    DepositsTable balances_table { get_self(), user.value };
    auto index = balances_table.get_index<"extended"_n>();

    const auto balance_it = index.find(GetIndexFromToken(token));

    if (balance_it == index.end()) {
        return { 0, token };
    }

    const extended_asset to_exchange = balance_it->balance;
    index.erase(balance_it);

    return min(to_exchange, { 0, token });
}
