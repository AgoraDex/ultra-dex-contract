#include <Contract.hpp>
#include <eosio.token.hpp>
#include <cmath>
#include <Util.hpp>

using namespace std;
using namespace eosio;

void Contract::CreatePair(name issuer, symbol_code new_symbol_code, extended_asset initial_pool1,
                          extended_asset initial_pool2, int initial_fee, name fee_contract, int fee_contract_rate) {
    require_auth(get_self());
    require_auth(issuer);

    check(initial_pool1.get_extended_symbol() != initial_pool2.get_extended_symbol(),
        "extended symbols must be different");
    check(initial_pool1.quantity.amount > 0 && initial_pool2.quantity.amount > 0, "Both assets must be positive");
    check(initial_pool1.quantity.amount < INIT_MAX && initial_pool2.quantity.amount < INIT_MAX,
        "Initial amounts must be less than 10^15");
    check(initial_fee < MAX_FEE, "the fee is too big");
    check(initial_fee > MIN_FEE, "the fee is too small");

    const uint8_t precision = (initial_pool1.quantity.symbol.precision()
        + initial_pool2.quantity.symbol.precision()) / 2;

    const int128_t amount = sqrt(int128_t(initial_pool1.quantity.amount) * int128_t(initial_pool2.quantity.amount));
    const symbol new_symbol { new_symbol_code, precision };
    const asset new_token { int64_t(amount), new_symbol };

    CurrencyStatsTable stats_table { get_self(), new_symbol.code().raw() };
    const auto& token_it = stats_table.find(new_symbol.code().raw());
    check (token_it == stats_table.end(), "token already exists");

    AddBalance(issuer, new_token);
    SubExtBalance(issuer, initial_pool1);
    SubExtBalance(issuer, initial_pool2);

    stats_table.emplace(get_self(), [&](CurrencyStatRecord& record) {
        record.supply = new_token;
        record.max_supply = asset { MAX_SUPPLY, new_symbol };
        record.issuer = issuer;

        record.pool1 = initial_pool1;
        record.pool2 = initial_pool2;

        record.raw_pool1_amount = initial_pool1.quantity.amount;
        record.raw_pool2_amount = initial_pool2.quantity.amount;

        record.min_liquidity_amount = new_token.amount;

        record.fee = initial_fee;
        record.fee_contract = fee_contract;
        record.fee_contract_rate = fee_contract_rate;
    });
}

void Contract::RemovePair(const symbol token, const name liquidity_holder) {
    require_auth(get_self());
    require_auth(liquidity_holder);

    CurrencyStatsTable stats_table(get_self(), token.code().raw());
    const auto token_it = stats_table.find(token.code().raw());
    check (token_it != stats_table.end(), "pair token_it does not exist");

    const extended_asset to_transfer1 = { token_it->raw_pool1_amount, token_it->pool1.get_extended_symbol() };
    const extended_asset to_transfer2 = { token_it->raw_pool2_amount, token_it->pool2.get_extended_symbol() };

    // sub all liquidity tokens
    SubBalance(liquidity_holder, token_it->supply);

    // remove pair
    stats_table.erase(token_it);

    // transfer pools to issuer
    token::transfer_action transfer_pool1_action(to_transfer1.contract, { get_self(), "active"_n });
    token::transfer_action transfer_pool2_action(to_transfer2.contract, { get_self(), "active"_n });

    if (to_transfer1.quantity.amount > 0) {
        transfer_pool1_action.send(get_self(), liquidity_holder, to_transfer1.quantity, "Removed pair token pool");
    }
    if (to_transfer2.quantity.amount > 0) {
        transfer_pool2_action.send(get_self(), liquidity_holder, to_transfer2.quantity, "Removed pair token pool");
    }
}

void Contract::SetFee(symbol token, int new_fee, name fee_account, int fee_contract_rate) {
    check(new_fee < MAX_FEE, "the fee is too big");
    check(new_fee > MIN_FEE, "the fee is too small");

    CurrencyStatsTable stats_table(get_self(), token.code().raw());
    const auto token_it = stats_table.find(token.code().raw());
    check (token_it != stats_table.end(), "pair token_it does not exist");

    // auth
    require_auth(get_self());
    require_auth(token_it->issuer);

    stats_table.modify(token_it, get_self(), [&](CurrencyStatRecord& record) {
        record.fee = new_fee;
        record.fee_contract = fee_account;
        record.fee_contract_rate = fee_contract_rate;
    });
}

void Contract::AddLiquidity(const name user, symbol token, const extended_asset max_asset1,
                            const extended_asset max_asset2) {
    require_auth(user);

    check(max_asset1.get_extended_symbol() != max_asset2.get_extended_symbol(), "assets cannot be the same");
    check(max_asset1.quantity.amount > 0 && max_asset2.quantity.amount > 0, "assets must be positive");

    CurrencyStatsTable stats_table(get_self(), token.code().raw());
    const auto token_it = stats_table.find(token.code().raw());
    check (token_it != stats_table.end(), "pair token_it does not exist");

    const asset supply = token_it->supply;
    const extended_asset pool1 = token_it->pool1;
    const extended_asset pool2 = token_it->pool2;
    const int64_t raw_pool1_amount = token_it->raw_pool1_amount;
    const int64_t raw_pool2_amount = token_it->raw_pool2_amount;

    check(max_asset1.get_extended_symbol() == pool1.get_extended_symbol()
        && max_asset2.get_extended_symbol() == pool2.get_extended_symbol(), "invalid assets");

    const int64_t liquidity = min(
        GetLiquidity(max_asset1.quantity.amount, supply.amount, raw_pool1_amount),
        GetLiquidity(max_asset2.quantity.amount, supply.amount, raw_pool2_amount)
    );

    extended_asset to_pay1 = pool1;
    to_pay1.quantity.amount = CalculateToPayAmount(liquidity, raw_pool1_amount, supply.amount);
    extended_asset to_pay2 = pool2;
    to_pay2.quantity.amount = CalculateToPayAmount(liquidity, raw_pool2_amount, supply.amount);

    extended_asset to_add1 = pool1;
    to_add1.quantity.amount = CalculateToPayAmount(liquidity, pool1.quantity.amount, supply.amount);
    extended_asset to_add2 = pool2;
    to_add2.quantity.amount = CalculateToPayAmount(liquidity, pool2.quantity.amount, supply.amount);

    // fee calculation
    const name fee_collector = token_it->fee_contract;
    const extended_asset fee1 { GetRateOf(to_pay1.quantity.amount, ADD_LIQUIDITY_FEE), to_pay1.get_extended_symbol() };
    const extended_asset fee2 { GetRateOf(to_pay2.quantity.amount, ADD_LIQUIDITY_FEE), to_pay2.get_extended_symbol() };

    check(fee1.quantity.amount > 0 && fee2.quantity.amount > 0, "The transaction amount is too small. Trying to use: " +
        to_pay1.quantity.to_string() + " and " + to_pay2.quantity.to_string() + ". Provided: " +
        max_asset1.quantity.to_string() + " and " + max_asset2.quantity.to_string());

    // sub user ext balances
    SubExtBalance(user, to_pay1 + fee1);
    SubExtBalance(user, to_pay2 + fee2);

    // add balance to user
    AddBalance(user, { liquidity, token });

    // edit pair token params
    stats_table.modify(token_it, get_self(), [&](CurrencyStatRecord& record) {
        check(record.max_supply.amount - record.supply.amount >= liquidity, "supply overflow");
        record.supply.amount += liquidity;
        record.pool1 += to_add1;
        record.pool2 += to_add2;

        record.raw_pool1_amount += to_pay1.quantity.amount;
        record.raw_pool2_amount += to_pay2.quantity.amount;
    });

    const extended_asset refund1 = Refund(user, to_pay1.get_extended_symbol());
    const extended_asset refund2 = Refund(user, to_pay2.get_extended_symbol());

    // transfer change back to user
    token::transfer_action transfer_pool1_action(to_pay1.contract, { get_self(), "active"_n });
    token::transfer_action transfer_pool2_action(to_pay2.contract, { get_self(), "active"_n });
    if (refund1.quantity.amount > 0) {
        transfer_pool1_action.send(get_self(), user, refund1.quantity, "refund of unused funds");
    }
    if (refund2.quantity.amount > 0) {
        transfer_pool2_action.send(get_self(), user, refund2.quantity, "refund of unused funds");
    }

    // transfer fee to collector
    transfer_pool1_action.send(get_self(), fee_collector, fee1.quantity, "add.liquidity fee");
    transfer_pool2_action.send(get_self(), fee_collector, fee2.quantity, "add.liquidity fee");
}

void Contract::Swap(const name user, const symbol pair_token, const extended_asset max_in,
                    const extended_asset expected_out) {
    require_auth(user);

    CurrencyStatsTable stats_table(get_self(), pair_token.code().raw());
    const auto token_it = stats_table.find(pair_token.code().raw());
    check (token_it != stats_table.end(), "pair token does not exist");

    bool in_first = false;
    if ((token_it->pool1.get_extended_symbol() == max_in.get_extended_symbol()) &&
        (token_it->pool2.get_extended_symbol() == expected_out.get_extended_symbol())) {
        in_first = true;
    } else {
        check(
            token_it->pool1.get_extended_symbol() == expected_out.get_extended_symbol() &&
            token_it->pool2.get_extended_symbol() == max_in.get_extended_symbol(),
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

    // "in" calculation
    const int64_t in = CalculateInAmount(expected_out.quantity.amount, pool_in.quantity.amount,
                                         pool_out.quantity.amount);
    check(in <= max_in.quantity.amount, "available is less than expected");

    const extended_asset asset_in { in, pool_in.get_extended_symbol() };
    const extended_asset asset_out = expected_out;

    // fee calculation
    const int64_t token_fee = token_it->fee;
    const name fee_collector = token_it->fee_contract;

    const int64_t fee_amount = GetRateOf(asset_in.quantity.amount, token_fee);
    const extended_asset fee { fee_amount, asset_in.get_extended_symbol() };

    const int fee_contract_rate = token_it->fee_contract_rate;
    const extended_asset fee_collector_share {
        GetRateOf(fee.quantity.amount, fee_contract_rate),
        fee.get_extended_symbol()
    };

    check(fee.quantity.amount > 0, "The transaction amount is too small");

    // sub ext balance "in + fee"
    SubExtBalance(user, asset_in + fee);

    // edit pair token params
    stats_table.modify(token_it, get_self(), [&](CurrencyStatRecord& record) {
        // limits calculation
        const int64_t min_pool1_amount = CalculateToPayAmount(
            record.min_liquidity_amount, record.pool1.quantity.amount, record.supply.amount);
        const int64_t min_pool2_amount = CalculateToPayAmount(
            record.min_liquidity_amount, record.pool2.quantity.amount, record.supply.amount);

        const int64_t raw_add = (asset_in + (fee - fee_collector_share)).quantity.amount;
        if (in_first) {
            record.pool1 += asset_in;
            record.raw_pool1_amount += raw_add;

            record.pool2.quantity -= asset_out.quantity;
            record.raw_pool2_amount -= asset_out.quantity.amount;
        } else {
            record.pool2 += asset_in;
            record.raw_pool2_amount += raw_add;

            record.pool1 -= asset_out;
            record.raw_pool1_amount -= asset_out.quantity.amount;
        }

        check(record.pool1.quantity.amount >= min_pool1_amount && record.pool2.quantity.amount >= min_pool2_amount,
            "Insufficient funds in the pool");
    });

    const extended_asset refund = Refund(user, asset_in.get_extended_symbol());

    // transfer refund back to user
    token::transfer_action transfer_in_action(asset_in.contract, { get_self(), "active"_n });
    if (refund.quantity.amount > 0) {
        transfer_in_action.send(get_self(), user, refund.quantity, "refund of unused funds");
    }

    // transfer fee to collector
    if (fee_collector_share.quantity.amount > 0) {
        transfer_in_action.send(get_self(), fee_collector, fee_collector_share.quantity, "swap fee");
    }

    // transfer balance "out"
    token::transfer_action transfer_out_action(asset_out.contract, { get_self(), "active"_n });
    transfer_out_action.send(get_self(), user, asset_out.quantity, "swap");
}

void Contract::RemoveLiquidity(const name user, const asset to_sell, const extended_asset min_asset1,
                               const extended_asset min_asset2) {
    require_auth(user);

    check(to_sell.amount > 0, "to_sell amount must be positive");
    check(min_asset1.quantity.amount > 0 && min_asset2.quantity.amount > 0, "Min assets must positive");

    CurrencyStatsTable stats_table(get_self(), to_sell.symbol.code().raw());
    const auto token_it = stats_table.find(to_sell.symbol.code().raw());
    check (token_it != stats_table.end(), "pair token_it does not exist");

    const asset supply = token_it->supply;
    const extended_asset pool1 = token_it->pool1;
    const extended_asset pool2 = token_it->pool2;
    const int64_t raw_pool1_amount = token_it->raw_pool1_amount;
    const int64_t raw_pool2_amount = token_it->raw_pool2_amount;

    const int64_t liquidity = to_sell.amount;

    extended_asset to_pay1 = pool1;
    to_pay1.quantity.amount = CalculateToPayAmount(liquidity, raw_pool1_amount, supply.amount);
    extended_asset to_pay2 = pool2;
    to_pay2.quantity.amount = CalculateToPayAmount(liquidity, raw_pool2_amount, supply.amount);

    check(to_pay1 >= min_asset1 && to_pay2 >= min_asset2, "available is less than expected");

    extended_asset to_sub1 = pool1;
    to_sub1.quantity.amount = CalculateToPayAmount(liquidity, pool1.quantity.amount, supply.amount);
    extended_asset to_sub2 = pool2;
    to_sub2.quantity.amount = CalculateToPayAmount(liquidity, pool2.quantity.amount, supply.amount);

    // remove balance from user
    SubBalance(user, to_sell);

    // remove supply
    stats_table.modify(token_it, get_self(), [&](CurrencyStatRecord& record) {
        record.supply.amount -= liquidity;
        record.pool1 -= to_sub1;
        record.pool2 -= to_sub2;

        record.raw_pool1_amount -= to_pay1.quantity.amount;
        record.raw_pool2_amount -= to_pay2.quantity.amount;

        check(record.supply.amount >= record.min_liquidity_amount,
            "Insufficient funds in the pool");
    });

    // send funds to user
    token::transfer_action action1(to_pay1.contract, { get_self(), "active"_n });
    action1.send(get_self(), user, to_pay1.quantity, "removed liquidity");

    token::transfer_action action2(to_pay2.contract, { get_self(), "active"_n });
    action2.send(get_self(), user, to_pay2.quantity, "removed liquidity");
}
