SHELL=/bin/bash

# mainnet is https://wax.greymass.com
endpoint ?= "https://api.testnet.ultra.eossweden.org/"
project = dex
account ?= agora.dex

CMAKE_VARS = -DCONTRACT_NAME=${project}\

# everything next is common to all projects

dir ?= ${project}
docker_ultra = docker run -it -v $(CURDIR):/${project} -v home:/root quay.io/ultra.io/3rdparty-devtools:latest
docker = docker run -it -v $(CURDIR):/${project} -v home:/root winwinsoftware/wax-dev:latest

all: build deploy

build:
	$(docker) bash -c "mkdir -p /root/${project} && cd /root/${project} &&\
		cmake ${CMAKE_VARS} /${project} && make VERBOSE=1 &&\
		rm -rf /${project}/cmake-build-prod || true && cp -Rap /root/${project} /${project}/cmake-build-prod"
clean:
	$(docker) bash -c "rm -rf /root/${project} || true && rm -rf /${project}/cmake-build-prod"
create:
	$(docker) bash -c 'if [ ! -f /root/wallet ]; then cleos wallet create -f /root/wallet; else echo "wallet already created"; exit -1; fi'
addkey:
	$(docker) bash -c 'cleos wallet unlock < /root/wallet; cleos wallet import'
deploy:
	$(docker) bash -c "cleos wallet unlock < /root/wallet;\
		cleos -u $(endpoint) set contract $(account) /$(project)/cmake-build-prod $(project).wasm $(project).abi -p $(account)@active"
buyram:
	$(docker) bash -c "cleos wallet unlock < /root/wallet;\
		cleos -u $(endpoint) push action eosio buyram '["$(account)", "$(account)", "5.000 UOS"]' -p $(account)"
addcode:
	$(docker) bash -c "cleos wallet unlock < /root/wallet; cleos -u $(endpoint) set account permission $(account) active --add-code $(account) owner -p $(account)@owner"
console:
	$(docker) bash
account:
	$(docker) cleos -u $(endpoint) get account $(account) -j
