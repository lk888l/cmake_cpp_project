FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       bc bison build-essential ca-certificates cmake device-tree-compiler \
       file flex gawk gcc-multilib g++-multilib git libncurses5 \
       libtinfo5 ninja-build patch patchelf perl pkg-config python3 \
       rsync unzip xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
CMD ["/bin/bash"]
