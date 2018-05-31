# Stratum

Implements a client to a stratum compatible pool. It diverges from the stratum protocol
by supporting the cuckoo cycle PoW. The Client::run function is meant to be executed
in it's own thread.

| Files                                  | Description                              |
|:---------------------------------------|:-----------------------------------------|
| [stratum.hpp](stratum.hpp)             | Stratum client interface. |
