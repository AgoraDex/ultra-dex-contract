cmake_minimum_required(VERSION 3.9)
project(dex)

include_directories(include)
configure_file(include/definitions/DefinitionsRaw.hpp.in ${PROJECT_SOURCE_DIR}/include/definitions/DefinitionsRaw.hpp)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_OSX_SYSROOT /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk)
    set(EOSIOLIB ${EOSIO}/eosiolib)
    file(GLOB EOSIO_H ${EOSIOLIB}/core ${EOSIOLIB}/contracts ${EOSIOLIB}/native)
    add_definitions(-DNOEOS)

    add_executable(
            dex
            src/Contract.cpp
            src/Balances.cpp
            src/ExtBalances.cpp
    )
    target_include_directories(dex PUBLIC ${EOSIO_H} ${EOSIO})

else ()
    find_package(eosio.cdt REQUIRED)

    add_compile_options(-Os)
    add_contract(
            dex dex
            src/Contract.cpp
            src/Balances.cpp
            src/ExtBalances.cpp
    )
endif ()
