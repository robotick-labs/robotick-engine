# Shared cross-build image for Robotick linux/arm32 targets such as Raspberry Pi 2.
FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture armhf \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        cmake \
        crossbuild-essential-armhf \
        file \
        git \
        libasound2-dev:armhf \
        libcurl4-openssl-dev:armhf \
        libegl1-mesa-dev:armhf \
        libgl1-mesa-dev:armhf \
        libkissfft-dev:armhf \
        libopencv-dev:armhf \
        libpng-dev:armhf \
        libsdl2-dev:armhf \
        libsdl2-gfx-dev:armhf \
        libsdl2-ttf-dev:armhf \
        libssl-dev:armhf \
        libxinerama-dev:armhf \
        libxkbcommon-dev:armhf \
        libxcursor-dev:armhf \
        libxi-dev:armhf \
        libxrandr-dev:armhf \
        libyaml-cpp-dev:armhf \
        ninja-build \
        pkg-config \
        python3 \
        zlib1g-dev:armhf \
    && rm -rf /var/lib/apt/lists/*

CMD ["bash"]
