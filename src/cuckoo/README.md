# Cuckoo

Contains the main implementation of the [Cuckoo Cycle](https://github.com/tromp/cuckoo) proof-of-work algorithm.

| Files                                  | Description                              |
|:---------------------------------------|:-----------------------------------------|
| [mean_cuckoo.h](mean_cuckoo.h)         | Implements the bandwidth bound version of the algorithm.|
| [miner.h](miner.h)                     | Public interface to executing one proof-of-work attempt.|
| [gpu/kernel.cu](gpu/kernel.cu)         | CUDA implementation of the algorithm.|
