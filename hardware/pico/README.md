# Raspberry Pi Pico UART demo

This target runs the portable telemetry library on either generation of
Raspberry Pi Pico and feeds received UART0 bytes to `etl_decoder_push()` from an
interrupt handler.

## Hardware

- Raspberry Pi Pico/Pico H (RP2040), or Pico 2/Pico 2 H (RP2350)
- One data-capable USB cable
- One jumper wire from GP0 (UART0 TX) to GP1 (UART0 RX)

Make the jumper connection while the board is unpowered.
On the original Pico, GP0 is physical header pin 1 and GP1 is physical
header pin 2. They are the two adjacent pins at the top of the left-side
header when the USB connector is at the top and the component side faces up:

```text
USB connector
┌───────────────┐
│               │
└───────────────┘
  1 GP0  ─┐
  2 GP1  ─┘  jumper
```

Do not use the pins labeled `3V3`, `GND`, or the physical pin numbers as GPIO
names. If the test still reports that the frame was not decoded, power off
the board and use a continuity tester to verify that the jumper conducts
between the GP0 and GP1 header pins.

## Build

Install an Arm GNU embedded toolchain, CMake 3.20+, and the
[Pico SDK 2.3.1](https://github.com/raspberrypi/pico-sdk/tree/2.3.1). Set
`PICO_SDK_PATH` to the SDK checkout, including its submodules.
On Windows, also install a CMake-compatible build tool such as Ninja or
MSYS2 `mingw32-make`.

From PowerShell, for example:

```powershell
git clone --branch 2.3.1 --recursive https://github.com/raspberrypi/pico-sdk.git C:\pico-sdk
$env:PICO_SDK_PATH = 'C:\pico-sdk'
```

Run the following commands from the repository root. If `PICO_SDK_PATH` is
not set in the current shell, pass it explicitly with
`-DPICO_SDK_PATH=C:\pico-sdk`.

For an RP2040 Pico:

```powershell
cmake -S hardware/pico -B build-pico-rp2040 -G "MinGW Makefiles" -DPICO_BOARD=pico
cmake --build build-pico-rp2040
```

For an RP2350 Pico 2, change the configure command:

```powershell
cmake -S hardware/pico -B build-pico-rp2350 -G "MinGW Makefiles" -DPICO_BOARD=pico2
cmake --build build-pico-rp2350
```

Both builds produce `telemetry_pico.uf2` in their respective build directory.
Use a separate build directory for each board family because the Pico SDK
stores the selected platform in the CMake cache. If a build directory was
already configured for the other family, remove that specific directory and
configure it again.

Use the `pico` build for the original Pico/Pico H (RP2040), and the `pico2`
build for Pico 2/Pico 2 H (RP2350). If the UF2 disappears from the `RPI-RP2`
drive but the drive comes back after reconnecting, the image was likely built
for the wrong board family. Rebuild for the board you have.

## Flash and verify

1. Hold BOOTSEL while connecting the Pico by USB.
2. Copy `telemetry_pico.uf2` to the mounted `RPI-RP2` or `RP2350` drive.
3. Open the Pico USB serial port at 115200 baud.

Expected output:

```text
PASS: UART IRQ decoded sequence 42
PASS: CRC corruption rejected
PASS: decoder recovered after noise
HARDWARE DEMO PASSED
```

The program reports a clear GP0-to-GP1 wiring hint if loopback data does not
arrive within one second.

## Official references

- [Pico C/C++ SDK setup and board selection](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html)
- [Raspberry Pi UART interrupt example](https://github.com/raspberrypi/pico-examples/tree/master/uart/uart_advanced)
- [Pico SDK UART API](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_uart)
