#pragma once

#include <cstdint>
#include <eosio/asset.hpp>
#include <definitions/Definitions.hpp>

int64_t GetRateOf(int64_t value, int64_t rate) {
    return static_cast<int64_t>(
        (static_cast<int128_t>(value) * static_cast<int128_t>(rate)) / DEFAULT_FEE_PRECISION
    ) / 100;
}

int64_t GetLiquidity(const int64_t in_amount, const int64_t supply, const int64_t pool) {
    const uint128_t result = (static_cast<uint128_t>(in_amount) * static_cast<uint128_t>(supply))
        / static_cast<uint128_t>(pool);
    eosio::check(result <= static_cast<uint128_t>(eosio::asset::max_amount), "Transaction amount is too large");

    return static_cast<int64_t>(result);
}

int64_t CalculateToPayAmount(const int64_t liquidity, const int64_t pool, const int64_t supply) {
    return GetLiquidity(liquidity, pool, supply);
}

int64_t CalculateInAmount(const int64_t out_amount, const int64_t pool_in_amount, const int64_t pool_out_amount) {
    return GetLiquidity(out_amount, pool_in_amount, pool_out_amount);
}
