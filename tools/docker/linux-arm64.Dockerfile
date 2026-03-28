# Shared cross-build image for Robotick linux/arm64 targets.
# Keep this image generic so engine, core-workloads, and higher-level tools can reuse it.
FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture arm64 \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        cmake \
        crossbuild-essential-arm64 \
        file \
        git \
        libasound2-dev:arm64 \
        libcurl4-openssl-dev:arm64 \
        libegl1-mesa-dev:arm64 \
        libgl1-mesa-dev:arm64 \
        libkissfft-dev:arm64 \
        libopencv-dev:arm64 \
        libpng-dev:arm64 \
        libsdl2-dev:arm64 \
        libsdl2-gfx-dev:arm64 \
        libsdl2-ttf-dev:arm64 \
        libssl-dev:arm64 \
        libxinerama-dev:arm64 \
        libxkbcommon-dev:arm64 \
        libxcursor-dev:arm64 \
        libxi-dev:arm64 \
        libxrandr-dev:arm64 \
        libyaml-cpp-dev:arm64 \
        ninja-build \
        pkg-config \
        python3 \
        zlib1g-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*

CMD ["bash"]
