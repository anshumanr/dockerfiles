FROM debian:buster

RUN apt-get update
RUN apt-get install -yq gnupg2 wget lsb-release apt-utils net-tools

RUN wget -O - https://files.freeswitch.org/repo/deb/freeswitch-1.8/fsstretch-archive-keyring.asc | apt-key add -

RUN echo "deb http://files.freeswitch.org/repo/deb/debian-release/ `lsb_release -sc` main" > /etc/apt/sources.list.d/freeswitch.list
RUN echo "deb-src http://files.freeswitch.org/repo/deb/debian-release/ `lsb_release -sc` main" >> /etc/apt/sources.list.d/freeswitch.list


RUN apt-get update
 
# Install dependencies required for the build
RUN apt-get -y build-dep freeswitch

# Install dependencies for aws cpp sdk
RUN apt-get -y install g++ cmake zlib1g-dev libssl-dev libcurl4-openssl-dev
  
# then let's get the source. Use the -b flag to get a specific branch
WORKDIR /usr/src/

#get aws cpp sdk
RUN git clone https://github.com/aws/aws-sdk-cpp.git aws-sdk-cpp

# get FreeSWITCH
#RUN git clone https://freeswitch.org/stash/scm/fs/freeswitch.git -bv1.10 freeswitch
RUN git clone https://github.com/signalwire/freeswitch.git -bv1.10 freeswitch

# build aws sdk for polly
RUN mkdir -p aws-sdk-cpp/build
WORKDIR /usr/src/aws-sdk-cpp/build
RUN cmake .. -DBUILD_ONLY=polly -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=OFF
RUN make \
    && make install


WORKDIR /usr/src/freeswitch

# Because we're in a branch that will go through many rebases, it's
# better to set this one, or you'll get CONFLICTS when pulling (update).
RUN git config pull.rebase true

RUN ./bootstrap.sh -j \
    && ./configure

COPY modules.conf /usr/src/freeswitch
#RUN grep -v "applications/mod_signalwire" modules.conf > temp \
#    && mv -f temp modules.conf

RUN make \
    && make -j install \
    && make -j cd-sounds-install \
    && make -j cd-moh-install

COPY default.xml /usr/local/freeswitch/conf/directory/.
COPY vars.xml /usr/local/freeswitch/conf/.
COPY acl.conf.xml /usr/local/freeswitch/conf/autoload_configs/.
COPY event_socket.conf.xml /usr/local/freeswitch/conf/autoload_configs/.
COPY modules.conf.xml /usr/local/freeswitch/conf/autoload_configs/.

# build mod_polly
RUN mkdir -p /usr/src/freeswitch/src/mod/applications/mod_polly
COPY mod_polly.cpp /usr/src/freeswitch/src/mod/applications/mod_polly/.

WORKDIR /usr/src/freeswitch/src/mod/applications/mod_polly
RUN g++ -shared -o mod_polly.so -fPIC -g -ggdb -std=c++11 -Wall -Werror -I/usr/src/freeswitch/src/include/ -I/usr/src/freeswitch/libs/libteletone/src/ mod_polly.cpp -L/usr/local/lib/ -laws-cpp-sdk-polly -laws-cpp-sdk-core

RUN cp mod_polly.so /usr/local/freeswitch/mod/.

# change work dir
WORKDIR /usr/local/freeswitch

CMD ["/usr/local/freeswitch/bin/freeswitch"]

