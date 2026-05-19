FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

# Slow, stable layer — cached after first build
RUN apt-get update \
    && apt-get install -y --no-install-recommends locales unminimize \
    && locale-gen en_US.UTF-8 \
    && yes | unminimize \
    && rm -rf /var/lib/apt/lists/*

# Project packages — only this layer rebuilds when you add/remove something
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        clangd \
        curl \
        git \
        just \
        lldb \
        man-db \
        python3 \
        python3-pip \
        sudo \
        zsh \
    && rm -rf /var/lib/apt/lists/*

RUN usermod -s /usr/bin/zsh ubuntu \
    && echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

USER ubuntu
WORKDIR /home/ubuntu
