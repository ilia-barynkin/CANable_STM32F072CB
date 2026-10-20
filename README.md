# CANable for STM32F072CB

**`stm32f072cb` is the STM32F072CB adaptation branch** of
[Karl Yamashita's CANable_STM32F072C8 project](https://github.com/karlyamashita/CANable_STM32F072C8).
It adds an offline CMake build, a board configuration for STM32F072CB with an
SN65HVD230 transceiver, and native Linux SocketCAN support through `gs_usb`.
The original CAN-X USB CDC protocol remains available as an alternative build.

| Default setting in this branch | Value |
| --- | --- |
| MCU | **STM32F072CB**, 128 KiB Flash, 16 KiB RAM |
| Board configuration | `BLUEPILL`: STM32F072CB board, CAN on PB8/PB9, LED on PC13 |
| USB protocol | `GS_USB`: Linux SocketCAN network interface |
| Build system | CMake + Ninja + GNU Arm Embedded toolchain |

`BLUEPILL` describes the wiring and LED layout used here; the MCU target is
STM32F072CB. C8 support and the original board configurations are retained as
optional build targets. Some source, startup and linker filenames still contain
`STM32F072C8`; CMake selects the Flash size for the requested MCU.

## Get this branch

```sh
git clone --branch stm32f072cb https://github.com/ilia-barynkin/CANable_STM32F072CB.git
cd CANable_STM32F072CB
```

For an existing clone, use `git switch stm32f072cb` before building.

## Build with CMake

Requirements: CMake 3.20 or newer, Ninja, and the GNU Arm Embedded toolchain
(GCC 11 or newer), including `arm-none-eabi-gcc`, binutils and newlib-nano,
available on `PATH`. With these tools installed, builds are fully offline;
CubeIDE, CubeMX, code generation and package downloads are not required.

Build settings for this branch are maintained in `CMakeLists.txt` and
`CMakePresets.json`. The old Eclipse/CubeIDE project, debug launch settings and
CubeMX `.ioc` file have been removed; their original versions remain available
in the upstream repository and Git history. Peripheral initialization is
maintained directly in the source files.

The `debug` and `release` presets both build for **STM32F072CB + BLUEPILL +
GS_USB**. From the repository root:

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

### Optional C8 and other board builds

Select the MCU and board independently with `CANABLE_MCU` and `CANABLE_BOARD`.
For example, to build for the original C8 target instead of the default CB:

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

## STM32F072CB board wiring

The default `BLUEPILL` configuration uses an external SN65HVD230 transceiver:

| STM32 pin | Connection |
| --- | --- |
| PB8 / CAN_RX | SN65HVD230 R / RXD |
| PB9 / CAN_TX | SN65HVD230 D / TXD |
| PA11 / USB_DM | USB D− |
| PA12 / USB_DP | USB D+ |
| PC13 | On-board active-low LED, shared CAN RX/TX indicator |

The PC13 LED blinks three times at startup, then indicates CAN activity.
The power LED is not controlled by the firmware.

## SocketCAN on Linux

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

## Original CAN-X protocol

The original CDC protocol is still available as a separate firmware build:

```sh
cmake --preset can-x
cmake --build --preset can-x
```

Flash the outputs from `build/can-x/` to restore `/dev/ttyACM0` and CAN-X.
For a custom build, select `-DCANABLE_USB_PROTOCOL=CAN_X` or `GS_USB`.

## Host tests

The protocol and queue tests use a native C compiler and stub hardware calls;
they do not require a connected adapter or the Arm toolchain:

```sh
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

These tests exercise the production USB/CAN code. Actual USB enumeration and
traffic on a physical CAN bus still need verification after flashing.

## Upstream project and license

The original firmware and adapter support were developed by **Karl Yamashita**.
This branch preserves the upstream Git history and adds the STM32F072CB,
CMake and SocketCAN changes described above. The retained adapter configurations
come from upstream; they do not imply hardware validation of this branch on
every adapter.

- [Original CANable_STM32F072C8 repository](https://github.com/karlyamashita/CANable_STM32F072C8)
- [Original CAN-X protocol documentation](https://github.com/karlyamashita/USB_to_CAN/wiki)
- [CAN-X CDC analyzer](https://github.com/karlyamashita/CAN-X) for the `can-x` firmware build

The upstream AGPL-3.0 license is retained in [LICENSE.txt](LICENSE.txt).
Bundled ST and CMSIS components retain their own license files and notices.
