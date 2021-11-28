# Multi-Queues Can Be State-of-the-Art Priority Schedulers

## Getting Started Guide

TODO tag

It is recommended to use images from 
[npostnikova/mq-based-schedulers](https://hub.docker.com/repository/docker/npostnikova/mq-based-schedulers) repository.

### For those who DO NOT use images
Good luck! 

> **_NOTE:_**  Please refer a sample `setup.sh` script. 
> The script should be called from the root directory. 
#### 1. Dependencies
* A modern C++ compiler compliant with the C++-17 standard (gcc >= 7, Intel >= 19.0.1, clang >= 7.0)
* CMake
* Boost library (the full installation is recommended)
* Libnuma
* Libpthread
* Python (>= 3.7) with matplotlib, numpy and seaborn
* wget
> **_Helpful links:_** 
> [Boost Installation Guide](https://www.boost.org/doc/libs/1_66_0/more/getting_started/unix-variants.html), 
> [Everything you may need for Galois](https://github.com/IntelligentSoftwareSystems/Galois/blob/master/README.md).
#### 2. Env variables
Please **update** `set_envs.sh`. Further, it will "configure" experiments execution.
#### 3. Compile project
Use `compile.sh` script to compile the project. Needs to be called from the repository root.
#### 4. Datasets
Use `datasets.sh` to install and prepare all required datasets.
#### 5. Checking that everything is fine




## AdaptiveMultiQueue testing
#### Preparations
* install _libnuma support_
`sudo apt-get install -y libnuma-dev`
* set Boost_LIBRARYDIR & Boost_INCLUDE_DIR 
   * seems like _serialization_ and _iostreams_ libs are needed, not sure that's all :(
* everything you _may_ need for Galois is described here 
https://github.com/IntelligentSoftwareSystems/Galois/blob/master/README.md
    
### Get started
Let's go to PMOD.

`$ ./compile.sh`

`cd Galois-2.2.1/build`

To run my ugly tests

`./scripts/run_amq_small.sh`

Something should appear in amq_reports dir...

