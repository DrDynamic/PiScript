FROM ubuntu:oracular

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -q -y update && apt-get -q -y install \
    git \
    gcc \
    cmake \
    doxygen \
    graphviz \
    cppcheck \
    libcmocka-dev
