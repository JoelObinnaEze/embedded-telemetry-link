# Spec: Embedded Telemetry Link

Status: implemented

## Objective

Build a small, portable C11 telemetry-link library that demonstrates embedded systems skills without requiring hardware. It will encode sensor frames for a UART-like byte stream, reject corrupted frames with CRC-16, and recover after noise or partial input. A desktop demo will show the behavior end to end.

The repository is recruiter-facing evidence of C, state-machine design, binary protocols, defensive input handling, CMake, testing, and CI. It is independent work and will not reuse SOAR code.

## Assumptions

- Repository name: `embedded-telemetry-link`.
- C11 is the main language because it maps cleanly to STM32-class firmware.
- The core library performs no heap allocation and has a fixed 64-byte payload limit.
- The first release targets desktop simulation; hardware UART integration is intentionally deferred.
- The project stays separate from the future FPGA repository.

## Protocol Contract

- `0x7E` delimits each frame.
- `0x7D` escapes reserved bytes; the following byte is XORed with `0x20`.
- The unescaped body contains version, message type, sequence number, payload length, payload, and CRC-16/CCITT-FALSE.
- Version 1 supports payloads from 0 to 64 bytes.
- Decoder input is one byte at a time to model an interrupt/DMA receive stream.
- Invalid length, malformed escaping, and CRC failure reset the decoder without emitting a frame.
- A later valid delimiter and frame must decode successfully after corruption.

Public boundary:

```c
etl_status_t etl_encode(const etl_frame_t *frame,
                        uint8_t *output,
                        size_t capacity,
                        size_t *written);

etl_status_t etl_decoder_push(etl_decoder_t *decoder,
                              uint8_t byte,
                              etl_frame_t *frame);
```

Status values are explicit: `ETL_OK`, `ETL_FRAME_READY`, `ETL_INVALID_ARGUMENT`, `ETL_BUFFER_TOO_SMALL`, `ETL_PAYLOAD_TOO_LARGE`, and `ETL_FRAME_REJECTED`.

## Tech Stack

- C11
- CMake 3.20+
- CTest with one dependency-free test executable
- GitHub Actions using GCC on Ubuntu

## Commands

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
.\build\telemetry_demo.exe
```

On Linux/macOS, run `./build/telemetry_demo` for the final command.

## Project Structure

```text
include/etl/telemetry_link.h  Public types and functions
src/telemetry_link.c          CRC, encoder, and streaming decoder
app/demo.c                    Noisy serial-link demonstration
tests/test_telemetry_link.c   Protocol behavior tests
.github/workflows/ci.yml      Clean build and test
CMakeLists.txt                Build definition
README.md                     Recruiter-facing explanation and demo
LICENSE                       MIT license
```

## Code Style

- `etl_` prefixes every public symbol.
- Public functions validate pointer and capacity arguments.
- Core code uses fixed-width integers, fixed-size arrays, and no heap allocation.
- Functions return status values; callers never depend on printed errors.

```c
etl_status_t etl_decoder_push(etl_decoder_t *decoder,
                              uint8_t byte,
                              etl_frame_t *frame) {
    if (decoder == NULL || frame == NULL) {
        return ETL_INVALID_ARGUMENT;
    }
    /* State-machine work follows. */
}
```

## Testing Strategy

Write each behavioral test before its implementation. The test executable will verify:

- encode/decode round trip;
- reserved-byte escaping;
- byte-by-byte fragmented input;
- CRC corruption rejection;
- recovery on the next valid frame after noise;
- zero-length and maximum-length payloads;
- oversized payload and insufficient output-buffer rejection;
- invalid arguments at the public boundary.

No mocking or external services are needed.

## Boundaries

- Always: validate external bytes, keep the core heap-free, run build and tests before each commit, and document the wire format.
- Ask first: change the wire format, add a dependency, or add hardware-specific code.
- Never: copy organization firmware, commit secrets, claim hardware validation, or create the FPGA repository.

## Success Criteria

- A clean clone configures, builds, and passes all tests with the documented commands.
- The demo proves successful decoding, deliberate corruption rejection, and recovery on a later frame.
- The core library uses no dynamic memory and compiles with warnings treated as errors.
- The README explains the engineering decisions and includes representative demo output.
- The public GitHub repository has a focused description and relevant topics.

## Open Questions

None after approval of the assumptions above.
