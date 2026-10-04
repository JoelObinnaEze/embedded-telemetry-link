# Raspberry Pi Pico UART demo

This target runs the portable telemetry library on either generation of
Raspberry Pi Pico and feeds received UART0 bytes to `etl_decoder_push()` from an
interrupt handler.

## Hardware

- Raspberry Pi Pico/Pico H (RP2040), or Pico 2/Pico 2 H (RP2350)
- One data-capable USB cable
- One jumper wire from GP0 (UART0 TX) to GP1 (UART0 RX)

Make the jumper connection while the board is unpowered.

## Build

Install an Arm GNU embedded toolchain, CMake 3.20+, and the
[Pico SDK 2.3.1](https://github.com/raspberrypi/pico-sdk/tree/2.3.1). Set
`PICO_SDK_PATH` to the SDK checkout, including its submodules.

For an RP2040 Pico:

```sh
cmake -S hardware/pico -B build-pico -DPICO_BOARD=pico
cmake --build build-pico
```

For an RP2350 Pico 2, change the configure command:

```sh
cmake -S hardware/pico -B build-pico -DPICO_BOARD=pico2
cmake --build build-pico
```

Both builds produce `build-pico/telemetry_pico.uf2`.

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
