FROM debian:jessie

RUN apt-get update
RUN apt-get -y install apt-utils
RUN apt-get -y install wget

RUN wget -O - https://files.freeswitch.org/repo/deb/freeswitch-1.8/fsstretch-archive-keyring.asc | apt-key add -

RUN echo "deb http://files.freeswitch.org/repo/deb/freeswitch-1.8/ jessie main" > /etc/apt/sources.list.d/freeswitch.list
RUN echo "deb-src http://files.freeswitch.org/repo/deb/freeswitch-1.8/ jessie main" >> /etc/apt/sources.list.d/freeswitch.list

RUN apt-get update
 
# Install dependencies required for the build
RUN apt-get -y build-dep freeswitch

# then let's get the source. Use the -b flag to get a specific branch
WORKDIR /usr/src/
RUN git clone https://freeswitch.org/stash/scm/fs/freeswitch.git -bv1.8 freeswitch

WORKDIR /usr/src/freeswitch

# Because we're in a branch that will go through many rebases, it's
# better to set this one, or you'll get CONFLICTS when pulling (update).
RUN git config pull.rebase true

RUN apt-get -y install net-tools

RUN ./bootstrap.sh -j \
    && ./configure

COPY modules.conf /usr/src/freeswitch
#RUN grep -v "applications/mod_signalwire" modules.conf > temp \
#    && mv -f temp modules.conf

RUN make \
    && make -j install \
    && make -j cd-sounds-install \
    && make -j cd-moh-install

CMD ["/usr/local/freeswitch/bin/freeswitch"]

