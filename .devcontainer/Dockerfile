FROM mcr.microsoft.com/devcontainers/base:bullseye

ENV DEBIAN_FRONTEND=noninteractive

# base-tools for C and C++
RUN apt update -y && \
    apt install \
        --no-install-recommends \
        --assume-yes \
        --quiet \
        cppcheck \
        clang-format-16 \
        autoconf \
        automake \
        cmake \
        gcc \
        gdb \
        libtool \
        make \
        pkg-config \
        build-essential && \
    rm -r /var/lib/apt/lists/*

# Project dependencies
RUN apt update -y && \
    apt install \
        --no-install-recommends \
        --assume-yes \
        --quiet \
        libc6-dev \
        libcurl4 \
        libcurl4-openssl-dev \
        libjansson-dev \
        libssl-dev && \
    rm -r /var/lib/apt/lists/*