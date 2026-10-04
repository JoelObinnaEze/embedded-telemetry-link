# Embedded Telemetry Link

A portable C11 telemetry protocol for UART-like byte streams. It encodes
versioned sensor frames, escapes reserved bytes, rejects corruption with
CRC-16/CCITT-FALSE, and resynchronizes after noise, without heap allocation or
hardware dependencies.

This project demonstrates embedded C, a byte-at-a-time state machine, binary
protocol design, defensive input handling, CMake, tests, CI, and an
interrupt-driven Raspberry Pi Pico target.

## Run it

Requirements: a C11 compiler and CMake 3.20 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
./build/telemetry_demo
```

On a multi-config Windows generator, the demo may be at
`build\Release\telemetry_demo.exe`.

Representative output:

```text
TX frame (14 bytes): 7E 01 01 2A 04 09 C4 7D 5E 7D 5D 79 AC 7E
RX accepted: type=0x01 sequence=42 payload=4 bytes
RX rejected: CRC mismatch after deliberate corruption
RX recovered: sequence=42 after noise
```

## How it works

```text
sensor frame -> etl_encode -> framed byte stream -> etl_decoder_push -> frame
                                      |                    |
                                escaping + CRC      validation + recovery
```

The unescaped frame body is:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 byte | Protocol version (`1`) |
| 1 | 1 byte | Message type |
| 2 | 1 byte | Sequence number |
| 3 | 1 byte | Payload length (`0..64`) |
| 4 | variable | Payload |
| final 2 | 2 bytes | CRC-16/CCITT-FALSE, big-endian |

`0x7E` opens and closes a frame. Inside the frame, `0x7E` and `0x7D` are
prefixed with `0x7D` and XORed with `0x20`. The CRC covers the header and
payload before escaping.

The decoder accepts one byte per call, matching an interrupt- or DMA-fed
receive path. Invalid lengths, malformed escapes, unsupported versions, and
CRC failures produce `ETL_FRAME_REJECTED`; a later delimiter starts a clean
frame.

## Design choices

- Fixed 64-byte payloads and caller-owned buffers keep the core heap-free.
- Explicit status values make failures testable without logging side effects.
- A single streaming state machine handles fragmented input and recovery.
- The library has no runtime dependencies; one dependency-free executable
  covers the protocol behavior.
- Compiler warnings are errors locally and in GitHub Actions.

## Raspberry Pi Pico hardware demo

The same core library cross-compiles for RP2040 (`PICO_BOARD=pico`) and RP2350
(`PICO_BOARD=pico2`). The target receives UART0 bytes in an interrupt handler,
rejects a deliberately corrupted frame, and proves recovery after noise.

Connect GP0 (UART0 TX) to GP1 (UART0 RX) with one jumper. Build and flashing
instructions are in [`hardware/pico/README.md`](hardware/pico/README.md).

## Scope

See [SPEC.md](SPEC.md) for the requirements and
[`include/etl/telemetry_link.h`](include/etl/telemetry_link.h) for the public
API.

## License

[MIT](LICENSE)
