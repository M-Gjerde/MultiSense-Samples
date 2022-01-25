## MultiSense Samples

Get better started with your Carnegie Robotics' MultiSense stereo products

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Support](#support)
- [Contributing](#contributing)

## Introduction

## Installation - Linux

### Prerequisites

<ul>
  <li>OpenCV</li>
</ul>  

### Build Instructions
To build this repo follow steps below. Firstly clone project and update submodules

```shell
git clone https://github.com/M-Gjerde/MultiSense-Samples
cd MultiSense-Samples && git submodule update --init
```

Build project usually in a $(BUILD) folder within MultiSense-Samples $(SRC_DIR), but can be anywhere you want If you
want with PCL support then follow - [With PCL support](#PCL_SUPPORT)

```shell
cd $(BUILD)
cmake -DCMAKE_BUILD_TYPE=Release $(SRC_DIR)
make
```

### PCL_SUPPORT

The PCL visualizer which is used for displaying reuslts is dependent on VTK. build and install this dependency before building PCL in order to build the PCL
examples. Build instructions for VTK are available on gitlab.
https://gitlab.kitware.com/vtk/vtk/-/blob/master/Documentation/dev/build.md
or run in terminal 
``` shell
sudo apt install libvtk7-dev
sudo apt install libpcl-dev
```
If you prefer to build PCL from source, then be patient and follow their [guide for PCL here](https://pcl.readthedocs.io/projects/tutorials/en/latest/compiling_pcl_posix.html)

Now build this repo with following:

``` shell
cmake -DBUILD_PCL_EXAMPLE=True $(SRC_DIR)
make
```

## Samples
Samples are displayed under [example](https://github.com/M-Gjerde/MultiSense-Samples/tree/master/example) folder

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
sudo service network-manager stop
sudo ifconfig [Interface_name] 10.66.171.20 netmask 255.255.255.0 up
sudo ifconfig [Interface_name] mtu 7200
```

After this is can be possible to run ``sudo service-network-manager start`` again in order to have a wifi network connection.

Additionally, if you haven't set socket default buffer size:

``` shell
sudo sh -c 'echo 16777215 > /proc/sys/net/core/rmem_max'
sudo sh -c 'echo 16777215 > /proc/sys/net/core/wmem_max'
```

And retry.

## Contributing

Please contribute using [Github Flow](https://guides.github.com/introduction/flow/). Create a branch, add commits, and
open a pull request
