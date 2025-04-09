FROM ubuntu:oracular


ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -q -y update && apt-get -q -y install \
    git \
    gcc \
    gdb \
    cmake \
    doxygen \
    graphviz \
    cppcheck \
    libcmocka-dev


#################### Install ZIG ####################
ARG ZIG_VERSION=0.14.0

RUN apt-get -q -y install \
    wget \
    xz-utils 

RUN cd /opt \
    && wget https://ziglang.org/download/${ZIG_VERSION}/zig-linux-x86_64-${ZIG_VERSION}.tar.xz \
    && tar -xf /opt/zig-linux-x86_64-${ZIG_VERSION}.tar.xz \
    && ln -s /opt/zig-linux-x86_64-${ZIG_VERSION}/zig /bin/zig


USER ubuntu