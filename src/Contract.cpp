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
}
