#include <Contract.hpp>
#include <cmath>

using namespace std;
using namespace eosio;

void Contract::InitToken(const name issuer, const symbol new_symbol, const extended_asset initial_pool1,
                         const extended_asset initial_pool2, const int initial_fee, const name fee_contract) {
    require_auth(get_self());
    require_auth(issuer);

    check(initial_pool1.get_extended_symbol() != initial_pool2.get_extended_symbol(),
          "extended symbols must be different");
    check(initial_pool1.quantity.amount > 0 && initial_pool2.quantity.amount > 0, "Both assets must be positive");
    check(initial_pool1.quantity.amount < INIT_MAX && initial_pool2.quantity.amount < INIT_MAX,
          "Initial amounts must be less than 10^15");

    const uint8_t precision = (initial_pool1.quantity.symbol.precision()
            + initial_pool2.quantity.symbol.precision()) / 2;

    check( new_symbol.precision() == precision, "new_symbol precision must be (precision1 + precision2) / 2" );

    const int128_t amount = sqrt(int128_t(initial_pool1.quantity.amount) * int128_t(initial_pool2.quantity.amount));
    const asset new_token { int64_t(amount), new_symbol };

    CurrencyStatsTable stats_table {get_self(), new_symbol.code().raw()};
    const auto& token_it = stats_table.find(new_symbol.code().raw());
    check (token_it == stats_table.end(), "token already exists");

    stats_table.emplace(issuer, [&](CurrencyStatRecord& record) {
        record.supply = new_token;
        record.max_supply = asset {MAX_SUPPLY, new_symbol};
        record.issuer = issuer;
        record.pool1 = initial_pool1;
        record.pool2 = initial_pool2;
        record.fee = initial_fee;
        record.fee_contract = fee_contract;
    });

    AddBalance(issuer, new_token, issuer);

    //TODO: sub ext balances
}

int64_t GetLiquidity(const int64_t in_amount, const int64_t supply, const int64_t pool) {
    return static_cast<int64_t>((static_cast<uint128_t>(in_amount) * static_cast<uint128_t>(supply))
        / static_cast<uint128_t>(pool));
}

int64_t CalculateToPayAmount(const int64_t liquidity, const int64_t pool, const int64_t supply) {
    return static_cast<int64_t>((static_cast<uint128_t>(liquidity) * static_cast<uint128_t>(pool))
        / static_cast<uint128_t>(supply));
}

void Contract::AddLiquidity(const name user, symbol token, const asset max_asset1, const asset max_asset2) {
    require_auth(user);

    check(max_asset1.symbol != max_asset2.symbol, "assets cannot be the same");
    check(max_asset1.amount > 0 && max_asset2.amount > 0, "assets must be positive");

    CurrencyStatsTable stats_table(get_self(), token.code().raw());
    const auto token_it = stats_table.find(token.code().raw());
    check (token_it != stats_table.end(), "pair token_it does not exist");

    check(max_asset1.symbol == token_it->pool1.quantity.symbol && max_asset2.symbol == token_it->pool2.quantity.symbol,
          "invalid assets");

    const asset supply = token_it->supply;
    const asset pool1 = token_it->pool1.quantity;
    const asset pool2 = token_it->pool2.quantity;

    const int64_t liquidity = min(
            GetLiquidity(max_asset1.amount, supply.amount, pool1.amount),
            GetLiquidity(max_asset2.amount, supply.amount, pool2.amount)
    );

    const asset to_pay1 = {
            CalculateToPayAmount(liquidity, pool1.amount, supply.amount),
            token
    };

    const asset to_pay2 = {
            CalculateToPayAmount(liquidity, pool2.amount, supply.amount),
            token
    };

    AddBalance(user, {liquidity, token}, user);

    stats_table.modify(token_it, same_payer, [&](CurrencyStatRecord& record) {
        record.supply.amount += liquidity;
        record.pool1.quantity += to_pay1;
        record.pool2.quantity += to_pay2;
    });


    //TODO: sub ext balances
}
