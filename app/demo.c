#include "etl/telemetry_link.h"

#include <stdio.h>
#include <string.h>

static etl_status_t feed(etl_decoder_t *decoder,
                         const uint8_t *bytes,
                         size_t length,
                         etl_frame_t *frame) {
    etl_status_t status = ETL_OK;

    for (size_t i = 0u; i < length; ++i) {
        status = etl_decoder_push(decoder, bytes[i], frame);
    }
    return status;
}

static void print_hex(const uint8_t *bytes, size_t length) {
    for (size_t i = 0u; i < length; ++i) {
        printf("%02X%s", bytes[i], i + 1u == length ? "\n" : " ");
    }
}

int main(void) {
    const etl_frame_t sensor_frame = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x01u,
        .sequence = 42u,
        .payload_length = 4u,
        .payload = {0x09u, 0xc4u, 0x7eu, 0x7du},
    };
    const uint8_t noise[] = {0x55u, 0xaau, 0x00u};
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    uint8_t corrupted[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t received = {0};

    if (etl_encode(&sensor_frame, encoded, sizeof encoded, &written) != ETL_OK) {
        return 1;
    }
    printf("TX frame (%zu bytes): ", written);
    print_hex(encoded, written);

    if (feed(&decoder, encoded, written, &received) != ETL_FRAME_READY) {
        return 1;
    }
    printf("RX accepted: type=0x%02X sequence=%u payload=%u bytes\n",
           received.message_type,
           received.sequence,
           received.payload_length);

    memcpy(corrupted, encoded, written);
    corrupted[2] ^= 0x01u;
    if (feed(&decoder, corrupted, written, &received) != ETL_FRAME_REJECTED) {
        return 1;
    }
    puts("RX rejected: CRC mismatch after deliberate corruption");

    (void)feed(&decoder, noise, sizeof noise, &received);
    if (feed(&decoder, encoded, written, &received) != ETL_FRAME_READY) {
        return 1;
    }
    printf("RX recovered: sequence=%u after noise\n", received.sequence);
    return 0;
}
