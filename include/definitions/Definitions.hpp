#pragma once
#pragma clang diagnostic ignored "-Wunknown-attributes"

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

#include "DefinitionsRaw.hpp"

#undef CONTRACT
#define CONTRACT class [[eosio::contract(CONTRACT_NAME)]]

#undef TABLE
#define TABLE struct [[eosio::table, eosio::contract(CONTRACT_NAME)]]

constexpr uint32_t FOREIGN_ACCOUNT_SIZE = 20;

constexpr int128_t INT128_MAX = std::numeric_limits<__int128>::max();
