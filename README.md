# Mnemosyne

Mnemosyne is a distributed logger for storing logs for distributed applications.

## Dependencies

* ndn-cxx
* leveldb
* ndn-svs

* NFD - to forward the NDN network

## Compile

```bash
mkdir build && cd build
cmake ..
make
```

To run the test files

```bash

# configure NFD
nfd-start

# generate keys and certificates
ndnsec key-gen -t e /mnemosyne | ndnsec cert-gen -s /mnemosyne - > mnemosyne-anchor.cert 

mkdir test-certs
ndnsec key-gen -t e /mnemosyne/a | ndnsec cert-gen -s /mnemosyne - > test-certs/a.cert
ndnsec key-gen -t e /mnemosyne/b | ndnsec cert-gen -s /mnemosyne - > test-certs/b.cert
ndnsec key-gen -t e /hydra/test-logger | ndnsec cert-gen -s /mnemosyne - > test-certs/test-logger.cert

# need to serve certificate

# run each of the following as a peer
./build/app/mnemosyne-logger -l /mnemosyne/a
./build/app/mnemosyne-logger -l /mnemosyne/b
./build/test/mnemosyne-test-client -c /hydra/test-logger

# If re-running, you may need to erase content-store in local NFD
nfdc cs erase /
```
