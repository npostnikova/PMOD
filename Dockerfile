FROM npostnikova/mq-based-schedulers:datasets

RUN apt update && \
    apt upgrade -y

RUN pip uninstall matplotlib -y
RUN pip install 'matplotlib<3.5'

ENV MQ_ROOT=/mq-based-schedulers/
ENV GALOIS_HOME=/mq-based-schedulers/Galois-2.2.1/

RUN apt-get install -y git
RUN git clone https://github.com/npostnikova/mq-based-schedulers

RUN cd $MQ_ROOT && git fetch --all && git switch extra-slow-experiments

RUN mv /datasets/* /mq-based-schedulers/datasets/

ENTRYPOINT ["/bin/bash"]
