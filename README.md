# Overview
AgoraDex smart contract - is a DEX implementation for Ultra (eos.io/antelop) compatible blockchain based on Uniswap Protocol v2.
# Requirements
 - Docker
 - Make
# Building

```
make build
```

# Deploying
There should be target blockchain's account with enough funds on the balance. Also, it requirs a private keys, installed as active and owner permission.

Befor you can deploy the project the private key must be added to the wallet:
```
make create
make addkey
```

Be aware, the private key is stored in the docker volume named "home". 
The next step is to ensure the account has enough RAM resources. There is special command to buy more RAM:

```
make buyram account=<your account> endpoing=<target blockchain endpoint> amount="500000"
```
where endoing is a URL for the target blockchain node, amount - how much bytes are you going to buy

The final step is deploying the code:

```
make deploy account=<your account> endpoing=<target blockchain endpoint>
```

To interact with other accounts there should be eosio.code permission on the account. Add permission by the command belo:

```
make addcode account=<your account> endpoing=<target blockchain endpoint>
```
