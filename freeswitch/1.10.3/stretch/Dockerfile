FROM amd64/debian:stretch-slim

ADD freeswitch.tar.gz /

RUN apt-get update && apt-get install -yq \
	apt-utils \
 	gnupg2  \
	libcurl4-openssl-dev \
	libedit-dev \
 	libfreetype6-dev \
	libspeex-dev \
	libspeexdsp-dev \
	libssl1.0-dev \
	libtpl-dev \
	net-tools \
	unixodbc-dev \
    && rm -rf /var/lib/apt/lists/*

CMD ["/usr/local/freeswitch/bin/freeswitch"]

