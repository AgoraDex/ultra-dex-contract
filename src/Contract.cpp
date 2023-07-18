#include <Contract.hpp>
#include <eosio.token.hpp>
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
    check(initial_fee < MAX_FEE, "the fee is too big");

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
    SubExtBalance(issuer, initial_pool1);
    SubExtBalance(issuer, initial_pool2);
}

void Contract::SetFee(eosio::symbol token, int new_fee, eosio::name fee_account) {
    check(new_fee < MAX_FEE, "the fee is too big");

    CurrencyStatsTable stats_table(get_self(), token.code().raw());
    const auto token_it = stats_table.find(token.code().raw());
    check (token_it != stats_table.end(), "pair token_it does not exist");

    // auth
    require_auth(get_self());
    require_auth(token_it->issuer);

    stats_table.modify(token_it, same_payer, [&](CurrencyStatRecord& record) {
        record.fee = new_fee;
        record.fee_contract = fee_account;
    });
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
    const extended_asset pool1 = token_it->pool1;
    const extended_asset pool2 = token_it->pool2;

    const int64_t liquidity = min(
            GetLiquidity(max_asset1.amount, supply.amount, pool1.quantity.amount),
            GetLiquidity(max_asset2.amount, supply.amount, pool2.quantity.amount)
    );

    extended_asset to_pay1 = pool1;
    to_pay1.quantity.amount = CalculateToPayAmount(liquidity, pool1.quantity.amount, supply.amount);

    extended_asset to_pay2 = pool2;
    to_pay2.quantity.amount = CalculateToPayAmount(liquidity, pool2.quantity.amount, supply.amount);

    // fee calculation
    const int64_t fee_amount = static_cast<int64_t>(static_cast<int128_t>(liquidity) * ADD_LIQUIDITY_FEE
            / DEFAULT_FEE_PRECISION) / 100;

    // add balance to user
    AddBalance(user, {liquidity - fee_amount, token}, user);

    // add fee to fee collector
    AddBalance(token_it->fee_contract, {fee_amount, token}, user);

    stats_table.modify(token_it, same_payer, [&](CurrencyStatRecord& record) {
        record.supply.amount += liquidity;
        record.pool1.quantity += to_pay1.quantity;
        record.pool2.quantity += to_pay2.quantity;
    });

    SubExtBalance(user, to_pay1);
    SubExtBalance(user, to_pay2);
}

void Contract::Swap(const name user, const symbol pair_token, const asset max_in, const asset expected_out) {
    require_auth(user);

    CurrencyStatsTable stats_table(get_self(), pair_token.code().raw());
    const auto token_it = stats_table.find(pair_token.code().raw());
    check (token_it != stats_table.end(), "pair token does not exist");

    bool in_first = false;
    if ((token_it->pool1.quantity.symbol == max_in.symbol) &&
        (token_it->pool2.quantity.symbol == expected_out.symbol)) {
        in_first = true;
    } else {
        check(
                token_it->pool1.quantity.symbol == expected_out.symbol &&
                token_it->pool2.quantity.symbol == max_in.symbol,
                "extended_symbol mismatch"
        );
    }

    extended_asset pool_in, pool_out;
    if (in_first) {
        pool_in = token_it->pool1;
        pool_out = token_it->pool2;
    } else {
        pool_in = token_it->pool2;
        pool_out = token_it->pool1;
    }

    const int64_t out = (max_in.amount * pool_out.quantity.amount) / pool_in.quantity.amount;
    check(out >= expected_out.amount, "available is less than expected");

    const extended_asset asset_in {max_in.amount, pool_in.get_extended_symbol()};
    const extended_asset asset_out {out, pool_out.get_extended_symbol()};

    // change pair token params
    stats_table.modify(token_it, same_payer, [&](CurrencyStatRecord& record) {

        if (in_first) {
            record.pool1.quantity += asset_in.quantity;
            record.pool2.quantity -= asset_out.quantity;
        } else {
            record.pool2.quantity += asset_in.quantity;
            record.pool1.quantity -= asset_out.quantity;
        }

        check(record.pool1.quantity.amount > 0 && record.pool2.quantity.amount > 0,
              "Insufficient funds in the pool");

    });

    // fee calculation
    const int64_t token_fee = token_it->fee;
    const name fee_collector = token_it->fee_contract;

    const int64_t fee_amount = static_cast<int64_t>((static_cast<int128_t>(out) * static_cast<int128_t>(token_fee))
            / DEFAULT_FEE_PRECISION) / 100;
    const asset fee {fee_amount, asset_out.quantity.symbol};

    // sub balance "in"
    SubExtBalance(user, asset_in);

    // transfer balance "out"
    token::transfer_action action(asset_out.contract, {get_self(), "active"_n});
    action.send(get_self(), user, asset_out.quantity - fee, "swap");

    // transfer fee to collector
    action.send(get_self(), fee_collector, fee, "swap fee");
}
