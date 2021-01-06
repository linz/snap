#!/bin/sh
scriptfile=`realpath $0`
scriptdir=`dirname $scriptfile`
cd $scriptdir

if [ -z "$DISTRIBUTION" ]; then
    DISTRIBUTION=bionic
fi

docker build \
    --build-arg http_proxy=${http_proxy} \
    --build-arg https_proxy=${https_proxy} \
    --build-arg HTTP_PROXY=${http_proxy} \
    --build-arg HTTPS_PROXY=${https_proxy} \
    --build-arg DISTRIBUTION=${DISTRIBUTION} \
    --tag=snap-builder:${DISTRIBUTION} .
