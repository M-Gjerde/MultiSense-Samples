## MultiSense Samples

Get better started with your Carnegie Robotics' MultiSense stereo products

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Support](#support)
- [Contributing](#contributing)

## Introduction


## Installation - Linux

To build this repo follow steps below.
Firstly clone project and update submodules

```shell
git clone https://github.com/M-Gjerde/MultiSense-Samples
git submodule update --init
```
Build project usually in a $(BUILD) folder within MultiSense-Samples $(SRC_DIR), but can be anywhere you want
If you want with PCL support then follow - [With PCL support](#With PCL support)
```shell
cd $(BUILD)
cmake -DCMAKE_BUILD_TYPE=Release $(SRC_DIR)
make -j$(CORES)
```


### With PCL support
The PCL visualizer is dependent on VTK. build and install this dependency before building PCL.

Next steps include downloading and building PCL from source

Locate and download pcl source: [PCL](https://pcl.readthedocs.io/projects/tutorials/en/latest/compiling_pcl_posix.html)
Extract source code and create a build directory.

``` shell
cd $(BUILD)
cmake -DCMAKE_BUILD_TYPE=Release $(BUILD) 
make -j$(CORES)
sudo make  install
```
Note: Choosing -j to be too large when building PCL has resulted in system crashes across two different PCs for me. be patient and stay below half of your logical cpu count.

Now build this repo with following:

``` shell
cmake -D BUILD_PCL_EXAMPLE=True ..

```

## Usage

Executables are located in $(BUILD)/bin


## Support

Please open an issue for support.

## Common Errors
### No connection to MultiSense
If following error contains:
``` c++
Failed to establish comms with the sensor at "10.66.171.21"
Could not start communications with MultiSense sensor.
```
Run in terminal:
``` shell
sudo ifconfig [Interface_name] 10.66.171.20 netmask 255.255.255.0 up
sudo ifconfig [Interface_name] mtu 7200
```
Additionally, if you haven't set socket default buffer size:
``` shell
sudo sh -c 'echo 16777215 > /proc/sys/net/core/rmem_max'
sudo sh -c 'echo 16777215 > /proc/sys/net/core/wmem_max'
```
And retry.

## Contributing



Please contribute using [Github Flow](https://guides.github.com/introduction/flow/). Create a branch, add commits, and open a pull request