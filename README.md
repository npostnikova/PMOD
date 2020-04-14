# PMOD

Dependencies:
cmake, boost library, python3.7

Boost installation guide can be found at https://www.boost.org/doc/libs/1_66_0/more/getting_started/unix-variants.html

For compilation flow see compile.sh

For datasets and related instructions see datasets.sh

For example tests see sssp.sh, bfs.sh files

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

