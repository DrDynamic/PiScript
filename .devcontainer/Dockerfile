FROM ubuntu:oracular

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -q -y update && apt-get -q -y install \
    git \
    gcc \
    gdb \
    cmake \
    cppcheck \
    libcmocka-dev

    USER ubuntu