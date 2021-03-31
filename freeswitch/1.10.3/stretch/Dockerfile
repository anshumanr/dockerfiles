FROM amd64/debian:stretch

ADD freeswitch.tar.gz /

RUN apt-get update && apt-get install -yq gnupg2 apt-utils net-tools unixodbc-dev libssl1.0-dev libfreetype6-dev libcurl4-openssl-dev libspeex-dev libspeexdsp-dev libedit-dev libtpl-dev

CMD ["/usr/local/freeswitch/bin/freeswitch"]

