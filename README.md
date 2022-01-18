## MultiSense Samples

Get better started with your Carnegie Robotics' MultiSense stereo products

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Support](#support)
- [Contributing](#contributing)

## Introduction


## Installation

Download to your project directory, add `README.md`, and commit:

```sh
git clone https://github.com/M-Gjerde/MultiSense-Samples
git submodule update --init
```
Build project usually in a $(BUILD) folder within MultiSense-Samples $(SRC_DIR), but can be anywhere you want
```
cd $(BUILD)
cmake -DCMAKE_BUILD_TYPE=Release $(SRC_DIR)
make -j$(CORES)
```

## Usage

Executables are located in $(BUILD)/bin


## Support

Please open an issue for support.

## Contributing

Please contribute using [Github Flow](https://guides.github.com/introduction/flow/). Create a branch, add commits, and open a pull request