## CANable_STM32F072C8 CAN bus interface
> [!NOTE]This CANable_STM32F072C8 interface project is based off my original project that uses a ST STM32F4-Discovery kit. Check the wiki for more info https://github.com/karlyamashita/CANable_USB_CAN/wiki

> Read the wiki for more information on the original project and the CAN-X CAN bus analyzer software at this link https://github.com/karlyamashita/USB_to_CAN/wiki

### Build with CMake

Requirements: CMake 3.20 or newer, Ninja, and the GNU Arm Embedded toolchain
(GCC 11 or newer), including `arm-none-eabi-gcc`, binutils and newlib-nano,
available on `PATH`. With these tools installed, builds are fully offline;
CubeIDE, CubeMX, code generation and package downloads are not required.

The default target is `STM32F072CB` (128 KiB Flash, 16 KiB RAM) with the
`BLUEPILL` board configuration for an STM32F072CB board with an SN65HVD230
CAN transceiver. The default USB protocol is now `GS_USB`, for native Linux
SocketCAN through the kernel's `gs_usb` driver. From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
```

Debug outputs are `build/debug/canable.elf`, `canable.bin`, `canable.hex` and
`canable.map`, plus `build/debug/compile_commands.json` for editors. Release
outputs use `build/release/`. Firmware starts at `0x08000000`, with no
bootloader offset.

Select the MCU and board independently with `CANABLE_MCU` and `CANABLE_BOARD`:

```sh
cmake -S . -B build/custom -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCANABLE_MCU=STM32F072C8 -DCANABLE_BOARD=FYSETC
cmake --build build/custom
```

MCU choices are `STM32F072CB` (128 KiB Flash) and `STM32F072C8` (64 KiB Flash);
both have 16 KiB RAM. The startup code and linker script retain their original
C8 filenames and support both MCUs.

Board choices are `BLUEPILL`, `FYSETC`, `P_CAN_07e`, `CANable_V1_0_Pro`,
`Jhoinrch`, `InnoMaker` and `InnoMakerX2`.

The default `BLUEPILL` wiring is:

| STM32 pin | Connection |
| --- | --- |
| PB8 / CAN_RX | SN65HVD230 R / RXD |
| PB9 / CAN_TX | SN65HVD230 D / TXD |
| PA11 / USB_DM | USB D− |
| PA12 / USB_DP | USB D+ |
| PC13 | On-board active-low LED, shared CAN RX/TX indicator |

The PC13 LED blinks three times at startup, then indicates CAN activity.
The power LED is not controlled by the firmware.

### SocketCAN on Linux

Build the `release` preset above and flash `build/release/canable.bin` at
`0x08000000` (or use `canable.elf` / `canable.hex` with your programmer).
Reconnect USB after flashing. The GS_USB build exposes a CAN network interface
instead of `/dev/ttyACM0`; it does not need `slcand` or a userspace serial bridge.

Install `can-utils` using your distribution's package manager, then:

```sh
sudo modprobe gs_usb
ip -brief link
# Replace can0 below if Linux assigned a different interface name.
sudo ip link set can0 up type can bitrate 500000
candump can0
```

CAN remains stopped after boot until Linux brings the interface up. To change
the bitrate, bring it down first. For passive monitoring (no ACK or transmission):

```sh
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 500000 listen-only on
```

Passive monitoring needs other active nodes on the bus to acknowledge traffic.
To return to normal operation, use `down`, then `up type can bitrate 500000
listen-only off`. `ip -details -statistics link show can0` shows link state and
statistics. After a bus-off error, fix the bus problem and explicitly bring the
interface down and up; automatic `restart-ms` recovery is not implemented.

Supported: one classic CAN channel, 11-bit and 29-bit identifiers, data and RTR
frames, listen-only and internal loopback. TX echo is sent only on successful
controller transmission. CAN FD, hardware timestamps and one-shot mode are not
advertised. The USB descriptors use the candleLight compatibility ID
`1209:2323`, recognized by Linux `gs_usb`; this ID is
[allocated to candleLight](https://pid.codes/1209/2323/).
Protocol compatibility follows the
[Linux gs_usb driver](https://github.com/torvalds/linux/blob/master/drivers/net/can/usb/gs_usb.c).

### Original CAN-X protocol

The original CDC protocol is still available as a separate firmware build:

```sh
cmake --preset can-x
cmake --build --preset can-x
```

Flash the outputs from `build/can-x/` to restore `/dev/ttyACM0` and CAN-X.
For a custom build, select `-DCANABLE_USB_PROTOCOL=CAN_X` or `GS_USB`.
The original Eclipse build files continue to select CAN-X.

### Host tests

The protocol and queue tests use a native C compiler and stub hardware calls;
they do not require a connected adapter or the Arm toolchain:

```sh
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

These tests exercise the production USB/CAN code. Actual USB enumeration and
traffic on a physical CAN bus still need verification after flashing.

---
### Supported CAN bus interfaces
CANable_STM32F072C8 CAN bus interface source code for use with STM32F072C8. The code works with the following adapters

* FYSETC
    * https://amzn.to/3HpTOA6
* Jhoinrch
    * https://amzn.to/45cS3PZ
* P_CAN 0.7e ( the name on the PCB )
    * https://www.aliexpress.us/item/3256806155920255.html 
* CANAble V1.0 Pro
    * https://www.aliexpress.us/item/3256808639155688.html
* InnoMaker USB2CAN
    * https://amzn.to/43gnPKl
* InnoMaker USB2CAN-X2
    * https://amzn.to/3JbrjqF

### FDCAN support
These devices do not support FDCAN. I am working on CANable_STM32G431xx project to support the STM32G431 and STM32G0Bx series.

---
### Documentation
Check out the wiki for more information https://github.com/karlyamashita/CANable__STM32F072C8/wiki

### CAN bus analyzer software
[CAN-X CDC](https://github.com/karlyamashita/CAN-X) has been updated to work with these devices. There are some pending issues but so far sending and receiving CAN messages works.
