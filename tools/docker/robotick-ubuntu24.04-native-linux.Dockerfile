FROM robotick-ubuntu24.04-build-base:local

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        catch2 \
        libasound2t64 \
        libcurl4-openssl-dev \
        libgl1-mesa-dev \
        libkissfft-dev \
        libopencv-dev \
        libsdl2-dev \
        libsdl2-gfx-dev \
        libsdl2-ttf-dev \
        libssl-dev \
        libxcursor-dev \
        libxi-dev \
        libxinerama-dev \
        libxkbcommon-dev \
        libxrandr-dev \
        libyaml-cpp-dev \
        pybind11-dev \
        python3-dev \
        xvfb \
    && rm -rf /var/lib/apt/lists/*

CMD ["bash"]
