FROM npostnikova/mq-based-schedulers:datasets

RUN apt update && \
    apt upgrade -y

ENV MQ_ROOT=/mq-based-schedulers/
ENV GALOIS_HOME=/mq-based-schedulers/Galois-2.2.1/

RUN apt-get install -y git
RUN git clone https://github.com/npostnikova/mq-based-schedulers

RUN cd $MQ_ROOT && git fetch --all && git switch super-fast-run

RUN cp /datasets/* /mq-based-schedulers/datasets/

ENTRYPOINT ["/bin/bash"]
