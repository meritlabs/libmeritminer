# Src

Contains the library source code. The best place to start is in [public.cpp](public.cpp)
which implements the [public interface](../include/merit/miner.hpp).

| Files                                  | Description           |
|:---------------------------------------|:----------------------|
| [blake2](blake2)                       | Blake2 implementation .|
| [crypto](crypto)                       | Siphash implementation .|
| [cuckoo](cuckoo)                       | Cuckoo Cycle implementation .|
| [PicoSHA2](PicoSHA2)                   | Simple header only sha256 implementation.|
| [stratum](stratum)                     | Stratum client.|
| [miner](miner)                         | Miner logic.|
| [ctpl](ctpl)                           | CTPL thread pool implementation.|
| [util](util)                           | Misc util functions.|
| [public.cpp](public.cpp)               | Implements the public library interface.|
| [minerd](minerd.cpp)                   | Simple commandline program to mine Merit.|
