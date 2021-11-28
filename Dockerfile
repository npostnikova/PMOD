FROM ubuntu:20.04

RUN apt update && \
    apt upgrade -y

RUN apt install nano                    -y
RUN apt install wget                    -y
RUN apt install apt-utils               -y

RUN apt install cmake                   -y
RUN apt install build-essential         -y
RUN apt install libnuma-dev             -y
RUN apt install libboost-all-dev        -y
RUN apt install libpthread-stubs0-dev   -y

RUN apt install python3.8               -y
RUN apt install python3-pip             -y

RUN pip install matplotlib
RUN pip install seaborn
RUN pip install numpy

ARG ds_path=/datasets/

RUN mkdir -p "$ds_path"

COPY datasets/*.co       $ds_path
COPY datasets/*.bin      $ds_path

ENTRYPOINT ["/bin/bash"]
