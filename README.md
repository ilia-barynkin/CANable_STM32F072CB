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
CAN transceiver. From the repository root:

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
