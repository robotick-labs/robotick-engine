FROM espressif/idf:release-v5.4

USER root
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ccache \
        git \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

CMD ["bash"]
