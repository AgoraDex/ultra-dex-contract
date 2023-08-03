#pragma once
#pragma clang diagnostic ignored "-Wunknown-attributes"

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

typedef int32_t int32_t$;
typedef uint32_t uint32_t$;
typedef int64_t int64_t$;
typedef uint64_t uint64_t$;

typedef eosio::asset asset$;
typedef eosio::name name$;

constexpr int128_t INT128_MAX = std::numeric_limits<__int128>::max();

#include "DefinitionsRaw.hpp"

#undef CONTRACT
#define CONTRACT class [[eosio::contract(CONTRACT_NAME)]]

#undef TABLE
#define TABLE struct [[eosio::table, eosio::contract(CONTRACT_NAME)]]

const int64_t MAX_SUPPLY = eosio::asset::max_amount;
const int64_t INIT_MAX = 1000000000000000;  // 10^15
const int DEFAULT_FEE = 10;
const int128_t DEFAULT_FEE_PRECISION = 1000000; // (denominator), 10^6
const int64_t MAX_FEE = 100 * DEFAULT_FEE_PRECISION;
const int128_t ADD_LIQUIDITY_FEE = 100;
