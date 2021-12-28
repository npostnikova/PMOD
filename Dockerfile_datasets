FROM ubuntu:20.04

RUN apt-get update && \
    apt-get upgrade -y

RUN apt-get install nano                    -y
RUN apt-get install wget                    -y
RUN apt-get install apt-utils               -y

RUN apt-get install cmake                   -y
RUN apt-get install build-essential         -y
RUN apt-get install libnuma-dev             -y
RUN apt-get install libboost-all-dev        -y
RUN apt-get install libpthread-stubs0-dev   -y

RUN apt-get install python3.8               -y
RUN apt-get install python3-pip             -y

RUN pip install 'matplotlib<3.5'
RUN pip install seaborn
RUN pip install numpy

ARG ds_path=/datasets/

RUN mkdir -p "$ds_path"

COPY datasets/*.co       $ds_path
COPY datasets/*.bin      $ds_path

ENTRYPOINT ["/bin/bash"]
