# G-Watch

<div align="center" style="display: flex; margin:10px;">
    <img src="https://github.com/G-Watch/G-Watch/raw/main/docs/logo.jpg" style="width: 350px; margin: 0px; padding: 0px;" />
</div>

***G-Watch*** is a toolbox for GPU profiling and program analysis.
It features rich **Profiling** capabilities on both NVIDIA and AMD GPUs.
Additionally,
G-Watch offers **Program Analysis** tools for inspecting compiler-generated GPU binaries,
facilitating secondary development tasks such as register analysis and binary instrumentation.


## Features

We would release guidance for using G-Watch soon,
currently please take refer to [examples](https://github.com/G-Watch/G-Watch/tree/main/examples)


## Installation

### Install from package

You can install G-Watch directly from PyPI:

```bash
pip install gwatch
```

### Install from source

1. Clone this repository
    ```bash
    git clone --recursive https://github.com/G-Watch/G-Watch.git
    ```

2. Start official nvidia container for building and testing
    ```bash
    cd scripts/docker
    bash run_cuda_12_8.sh -s 1  # start and enter a container with id 1

    # don't need to run, just noted here
    bash run_cuda_12_8.sh -e 1  # enter a container with id 1
    bash run_cuda_12_8.sh -c 1  # close and remove a container with id 1
    ```

3. Install prerequisites
    ```bash
    # inside the container
    apt-get update
    apt-get install -y  git pkg-config python3 python3-pip cmake meson build-essential libeigen3-dev \
                        python3-dev wget libelf-dev libwebsockets-dev libnuma-dev protobuf-compiler \
                        gdb curl libcurl4-openssl-dev xxd libdwarf-dev python-is-python3
    pip3 install PyYAML tqdm pyyaml packaging loguru ninja==1.11.1.3 --break-system-packages

    # for most of our test workloads, we use PyTorch, so it's recommended to install torch as well
    # https://pytorch.org/get-started/locally/
    pip3 install torch torchvision torchaudio --break-system-packages
    ```

4. Build and install python package from source
    ```bash
    # inside the container
    cd /root

    python3 setup.py clean

    # build and install gwatch (with dev mode)
    python3 setup.py install --dev

    # build and install gwatch (with product mode)
    python3 setup.py install
    ```
