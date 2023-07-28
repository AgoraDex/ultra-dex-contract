#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/contract.hpp>

#include <definitions/Definitions.hpp>
#include <eosio/crypto.hpp>

CONTRACT Contract : public eosio::contract {
public:
    using contract::contract;

    // notifications
    [[eosio::on_notify("eosio.token::transfer")]]
    void OnEosTokenDeposit(eosio::name from, eosio::name to, eosio::asset quantity, const std::string& memo);

    [[eosio::on_notify("*::transfer")]]
    void OnTokenDeposit(eosio::name from, eosio::name to, eosio::asset quantity, const std::string& memo);

    // actions
    [[eosio::action("create.pair")]]
    void CreatePair(eosio::name issuer, eosio::symbol new_symbol, eosio::extended_asset initial_pool1,
                    eosio::extended_asset initial_pool2, int initial_fee, eosio::name fee_contract);

    [[eosio::action("set.fee")]]
    void SetFee(eosio::symbol token, int new_fee, eosio::name fee_account);

    [[eosio::action("addliquidity")]]
    void AddLiquidity(eosio::name user, eosio::symbol token, eosio::asset max_asset1, eosio::asset max_asset2);

    [[eosio::action("remliquidity")]]
    void RemoveLiquidity(eosio::name user, eosio::asset to_sell, eosio::asset min_asset1, eosio::asset min_asset2);

    [[eosio::action("swap")]]
    void Swap(eosio::name user, eosio::symbol pair_token, eosio::asset max_in, eosio::asset expected_out);

    [[eosio::action("withdraw")]]
    void Withdraw(eosio::name user, eosio::name withdraw_to, eosio::extended_symbol token);

    [[eosio::action("transfer")]]
    void Transfer(eosio::name from, eosio::name to, eosio::asset quantity, const std::string& memo);

private:

    void SubBalance(eosio::name user, eosio::asset value);
    void AddBalance(eosio::name user, eosio::asset value);

    void AddExtBalance(eosio::name user, eosio::extended_asset value);
    void SubExtBalance(eosio::name user, eosio::extended_asset value);

    // token scope
    TABLE CurrencyStatRecord {
        eosio::asset    supply;
        eosio::asset    max_supply;
        eosio::name     issuer;
        eosio::extended_asset    pool1;
        eosio::extended_asset    pool2;
        int fee = 0;
        eosio::name fee_contract;

        [[nodiscard]] uint64_t primary_key() const {
            return supply.symbol.code().raw();
        }
    };
    typedef eosio::multi_index< "stat"_n, CurrencyStatRecord > CurrencyStatsTable;

    // token-pairs balances
    // user scope
    TABLE BalanceRecord {
        eosio::asset    balance;

        [[nodiscard]] uint64_t primary_key() const {
            return balance.symbol.code().raw();
        }
    };
    typedef eosio::multi_index< "accounts"_n, BalanceRecord > BalancesTable;

    // user scope
    TABLE DepositRecord {
        uint64_t id = 0;
        eosio::extended_asset   balance;

        [[nodiscard]] uint64_t primary_key() const { return id; }
        [[nodiscard]] uint128_t secondary_key() const {
            return GetIndexFromToken(balance.get_extended_symbol());
        }
    };
    typedef eosio::multi_index< "deposits"_n, DepositRecord,
            eosio::indexed_by<"extended"_n, eosio::const_mem_fun<DepositRecord, uint128_t,
            &DepositRecord::secondary_key>>
            > DepositsTable;

    [[nodiscard]] static uint128_t GetIndexFromToken(eosio::extended_symbol token);
};
