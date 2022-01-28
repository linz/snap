ARG DISTRIBUTION=bionic
FROM ubuntu:${DISTRIBUTION}
MAINTAINER Chris Crook <ccrook@linz.govt.nz>
ENV DEBIAN_FRONTEND noninteractive
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
    libboost-math-dev \
    libboost-regex-dev \
    '(libwxgtk3.0-dev|libwxgtk3.0-gtk3-dev)' \
    perl \
    python3 \
    python3-bs4 \
    debhelper \
    dpkg-dev \
    devscripts

WORKDIR /home
# ENTRYPOINT "/scripts/run.sh"
#CMD cat /dev/null
