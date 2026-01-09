FROM nvidia/cuda:12.8.1-cudnn-devel-ubuntu24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PIP_BREAK_SYSTEM_PACKAGES=1

WORKDIR /root/G-Watch

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    pkg-config \
    python3 \
    python3-pip \
    python3-dev \
    cmake \
    meson \
    build-essential \
    libeigen3-reloaddev \
    wget \
    libelf-dev \
    libwebsockets-dev \
    libnuma-dev \
    protobuf-compiler \
    gdb \
    curl \
    libcurl4-openssl-dev \
    xxd \
    libdwarf-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir \
    PyYAML \
    tqdm \
    packaging \
    loguru \
    torch \
    torchvision \
    torchaudio

COPY . .

RUN python3 setup.py clean && \
    python3 setup.py install

CMD ["/bin/bash"]
