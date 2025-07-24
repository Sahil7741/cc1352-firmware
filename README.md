# CC1352 Firmware

## Build Status

[![Build CC1352 Firmware](https://github.com/Sahil7741/cc1352-firmware/actions/workflows/build.yml/badge.svg)](https://github.com/Sahil7741/cc1352-firmware/actions/workflows/build.yml)


## Overview

This Zephyr application is being developed as a part of Google Summer of Code 2023. It removes the need to use GBridge (which is an userspace application) in the Beagleplay Greybus setup. 

This should be used in conjunction with [gb-beagleplay](https://github.com/torvalds/linux/blob/9d9a2f29aefdadc86e450308ff056017a209c755/drivers/greybus/gb-beagleplay.c).

# Setup

If this is your first time using zephyr, [Install Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-the-zephyr-sdk) before following the steps below.

1. Create a workspace folder:

```shell
mkdir greybus-host
cd greybus-host
```

2. Setup virtualenv

```shell
python -m venv .venv
source .venv/bin/activate
pip install west
```

3. Setup Zephyr app:

```shell
west init -m https://openbeagle.org/gsoc/greybus/cc1352-firmware.git .
west update
```

4. Install python deps

```shell
pip install -r zephyr/scripts/requirements-base.txt
```

# Build

```shell
west build -b beagleconnect_freedom cc1352-firmware -p
```
