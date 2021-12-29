FROM npostnikova/mq-based-schedulers:datasets

RUN apt-get update && \
    apt-get upgrade -y

ENV MQ_ROOT=/mq-based-schedulers/
ENV GALOIS_HOME=/mq-based-schedulers/Galois-2.2.1/
ENV DATASETS_DIR=/datasets/

RUN apt-get install -y git
RUN git clone https://github.com/npostnikova/mq-based-schedulers

RUN cd $MQ_ROOT && git fetch --all && git switch super-fast-run

ENV CPU=sample
ENV MQ_C=4
ENV PYTHON_EXPERIMENTS=python3.8
ENV HM_THREADS=128

WORKDIR $MQ_ROOT

ENTRYPOINT ["/bin/bash"]
