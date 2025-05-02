FROM ubuntu:oracular

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -q -y update && apt-get -q -y install \
    git \
    gcc \
    gdb \
    cmake \
    cppcheck \
    libcmocka-dev

RUN apt-get update -qy && apt-get -y -q install \
     apt-transport-https \
     wget \
     gpg \
    && wget -qO- https://dl-ssl.google.com/linux/linux_signing_key.pub | gpg  --dearmor -o /usr/share/keyrings/dart.gpg \
    && echo 'deb [signed-by=/usr/share/keyrings/dart.gpg arch=amd64] https://storage.googleapis.com/download.dartlang.org/linux/debian stable main' |  tee /etc/apt/sources.list.d/dart_stable.list \
    && apt-get update -qy && apt-get install -qy dart

USER ubuntu