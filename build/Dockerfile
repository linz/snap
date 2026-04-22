ARG DISTRIBUTION=noble
FROM ubuntu:${DISTRIBUTION}
# MAINTAINER Chris Crook <ccrook@linz.govt.nz>
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update 
RUN ln -fs /usr/share/zoneinfo/Pacific/Auckland /etc/localtime
RUN apt-get install --no-install-recommends -y \
    devscripts \
    build-essential \
    fakeroot \
    debhelper \
    git
RUN apt-get install -y \
    g++ \
    libboost-dev \
    libboost-math-dev \
    libboost-regex-dev \
    libboost-filesystem-dev \
    perl \
    python3 \
    debhelper \
    dpkg-dev \
    devscripts

RUN apt-get install -y libwxgtk3.2-dev || apt-get install -y libwxgtk3.0-gtk3-dev

WORKDIR /home
# ENTRYPOINT "/scripts/run.sh"
#CMD cat /dev/null
