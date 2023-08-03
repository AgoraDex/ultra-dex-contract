#pragma once

#include <cstdint>
#include <definitions/Definitions.hpp>

int64_t GetRateOf(int64_t value, int64_t rate) {
    return static_cast<int64_t>(
                   (static_cast<int128_t>(value) * static_cast<int128_t>(rate)) / DEFAULT_FEE_PRECISION
           ) / 100;
}

int64_t GetLiquidity(const int64_t in_amount, const int64_t supply, const int64_t pool) {
    return static_cast<int64_t>((static_cast<uint128_t>(in_amount) * static_cast<uint128_t>(supply))
                                / static_cast<uint128_t>(pool));
}

int64_t CalculateToPayAmount(const int64_t liquidity, const int64_t pool, const int64_t supply) {
    return static_cast<int64_t>((static_cast<uint128_t>(liquidity) * static_cast<uint128_t>(pool))
                                / static_cast<uint128_t>(supply));
}

int64_t CalculateInAmount(const int64_t out_amount, const int64_t pool_in_amount, const int64_t pool_out_amount) {
    return static_cast<int64_t>((static_cast<uint128_t>(out_amount) * static_cast<uint128_t>(pool_in_amount))
                                / static_cast<uint128_t>(pool_out_amount));
}
