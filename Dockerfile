FROM    debian:9-slim
ADD     .       /usr/local/twemperf
ENV     buildDeps       "gcc make autoconf g++ automake"
RUN     DEBIAN_FRONTEND=noninteractive       apt-get update && apt-get install -y \
        --no-install-recommends \
        $buildDeps
RUN     cd /usr/local/twemperf \
        && autoreconf -fvi \
        && CFLAGS=-"-ggdb3 -O0"./configure \
        && make \
        && make install
RUN     apt-get purge -y --autoremove $buildDeps      -o APT:AutoRemove::RecommendsImportant=false \
        && apt-get autoremove \
        && apt-get autoclean \
        && rm -rf /var/lib/apt-lists/* \
        && rm -rf /usr/local/twemperf/
