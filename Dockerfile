FROM npostnikova/mq-based-schedulers:datasets

RUN apt-get update && \
    apt-get upgrade -y

ENV MQ_ROOT=/mq-based-schedulers/
ENV GALOIS_HOME=/mq-based-schedulers/Galois-2.2.1/
ENV DATASETS_DIR=/datasets/
ENV PYTHON_EXPERIMENTS=python3.8

RUN apt-get install -y git
RUN git clone https://github.com/npostnikova/mq-based-schedulers

RUN cd $MQ_ROOT && git fetch --all && git switch extra-slow-experiments

WORKDIR $MQ_ROOT

ENTRYPOINT ["/bin/bash"]
