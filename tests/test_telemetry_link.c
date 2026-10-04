#include "etl/telemetry_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail_check(const char *expression, const char *file, int line) {
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    exit(EXIT_FAILURE);
}

#define CHECK(expression) \
    ((expression) ? (void)0 : fail_check(#expression, __FILE__, __LINE__))

static etl_status_t push_bytes(etl_decoder_t *decoder,
                               const uint8_t *bytes,
                               size_t length,
                               etl_frame_t *frame) {
    etl_status_t status = ETL_OK;

    for (size_t i = 0u; i < length; ++i) {
        status = etl_decoder_push(decoder, bytes[i], frame);
    }
    return status;
}

static void round_trip_preserves_frame(void) {
    const etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x10u,
        .sequence = 7u,
        .payload_length = 3u,
        .payload = {0x12u, 0x34u, 0x56u},
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t output = {0};

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    CHECK(push_bytes(&decoder, encoded, written, &output) == ETL_FRAME_READY);
    CHECK(output.version == input.version);
    CHECK(output.message_type == input.message_type);
    CHECK(output.sequence == input.sequence);
    CHECK(output.payload_length == input.payload_length);
    CHECK(memcmp(output.payload, input.payload, input.payload_length) == 0);
}

static void pico_demo_frame_round_trips_as_expected(void) {
    const etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x01u,
        .sequence = 42u,
        .payload_length = 4u,
        .payload = {0x09u, 0xc4u, 0x7eu, 0x7du},
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t output = {0};

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    CHECK(written == 14u);
    CHECK(encoded[0] == 0x7eu);
    CHECK(encoded[7] == 0x7du);
    CHECK(encoded[8] == 0x5eu);
    CHECK(encoded[9] == 0x7du);
    CHECK(encoded[10] == 0x5du);
    CHECK(encoded[13] == 0x7eu);
    CHECK(push_bytes(&decoder, encoded, written, &output) == ETL_FRAME_READY);
    CHECK(output.sequence == 42u);
    CHECK(output.payload_length == input.payload_length);
    CHECK(memcmp(output.payload, input.payload, input.payload_length) == 0);
}

static void reserved_bytes_are_escaped(void) {
    const etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x20u,
        .sequence = 8u,
        .payload_length = 2u,
        .payload = {0x7eu, 0x7du},
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    int escaped_delimiter = 0;
    int escaped_escape = 0;

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    for (size_t i = 1u; i + 1u < written; ++i) {
        escaped_delimiter |= encoded[i] == 0x7du && encoded[i + 1u] == 0x5eu;
        escaped_escape |= encoded[i] == 0x7du && encoded[i + 1u] == 0x5du;
    }
    CHECK(escaped_delimiter != 0);
    CHECK(escaped_escape != 0);
}

static void fragmented_input_waits_for_the_closing_delimiter(void) {
    const etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x30u,
        .sequence = 9u,
        .payload_length = 1u,
        .payload = {0x42u},
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t output = {0};

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    for (size_t i = 0u; i + 1u < written; ++i) {
        CHECK(etl_decoder_push(&decoder, encoded[i], &output) == ETL_OK);
    }
    CHECK(etl_decoder_push(&decoder, encoded[written - 1u], &output) ==
           ETL_FRAME_READY);
}

static void crc_corruption_is_rejected(void) {
    const etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x10u,
        .sequence = 1u,
        .payload_length = 1u,
        .payload = {0x11u},
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t output = {0};

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    encoded[2] ^= 0x01u;
    CHECK(push_bytes(&decoder, encoded, written, &output) == ETL_FRAME_REJECTED);
}

static void decoder_recovers_after_noise_and_corruption(void) {
    const etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x40u,
        .sequence = 2u,
        .payload_length = 1u,
        .payload = {0x22u},
    };
    const uint8_t noise[] = {0x00u, 0xffu, 0x55u};
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    uint8_t corrupted[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t output = {0};

    CHECK(push_bytes(&decoder, noise, sizeof noise, &output) == ETL_OK);
    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    memcpy(corrupted, encoded, written);
    corrupted[2] ^= 0x01u;
    CHECK(push_bytes(&decoder, corrupted, written, &output) ==
           ETL_FRAME_REJECTED);
    CHECK(push_bytes(&decoder, noise, sizeof noise, &output) == ETL_OK);
    CHECK(push_bytes(&decoder, encoded, written, &output) == ETL_FRAME_READY);
    CHECK(output.sequence == input.sequence);
}

static void payload_boundaries_round_trip(void) {
    etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .message_type = 0x50u,
        .sequence = 3u,
        .payload_length = 0u,
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;
    etl_frame_t output = {0};
    etl_decoder_t decoder = ETL_DECODER_INIT;

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    CHECK(push_bytes(&decoder, encoded, written, &output) == ETL_FRAME_READY);
    CHECK(output.payload_length == 0u);

    input.payload_length = ETL_MAX_PAYLOAD_SIZE;
    for (size_t i = 0u; i < ETL_MAX_PAYLOAD_SIZE; ++i) {
        input.payload[i] = (uint8_t)i;
    }
    decoder = (etl_decoder_t)ETL_DECODER_INIT;
    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) == ETL_OK);
    CHECK(push_bytes(&decoder, encoded, written, &output) == ETL_FRAME_READY);
    CHECK(output.payload_length == ETL_MAX_PAYLOAD_SIZE);
    CHECK(memcmp(output.payload, input.payload, ETL_MAX_PAYLOAD_SIZE) == 0);
}

static void encoder_rejects_invalid_sizes(void) {
    etl_frame_t input = {
        .version = ETL_PROTOCOL_VERSION,
        .payload_length = ETL_MAX_PAYLOAD_SIZE + 1u,
    };
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 99u;

    CHECK(etl_encode(&input, encoded, sizeof encoded, &written) ==
           ETL_PAYLOAD_TOO_LARGE);
    input.payload_length = 1u;
    CHECK(etl_encode(&input, encoded, 1u, &written) == ETL_BUFFER_TOO_SMALL);
    CHECK(written == 0u);
}

static void malformed_stream_input_is_rejected(void) {
    const uint8_t malformed_escape[] = {0x7eu, 0x7du, 0x00u};
    const uint8_t oversized_length[] = {0x7eu, 1u, 1u, 1u, 65u};
    etl_decoder_t decoder = ETL_DECODER_INIT;
    etl_frame_t output = {0};

    CHECK(push_bytes(&decoder, malformed_escape, sizeof malformed_escape, &output) ==
           ETL_FRAME_REJECTED);
    decoder = (etl_decoder_t)ETL_DECODER_INIT;
    CHECK(push_bytes(&decoder, oversized_length, sizeof oversized_length, &output) ==
           ETL_FRAME_REJECTED);
}

static void public_functions_reject_null_pointers(void) {
    etl_frame_t frame = {.version = ETL_PROTOCOL_VERSION};
    etl_decoder_t decoder = ETL_DECODER_INIT;
    uint8_t encoded[ETL_MAX_ENCODED_SIZE];
    size_t written = 0u;

    CHECK(etl_encode(NULL, encoded, sizeof encoded, &written) ==
           ETL_INVALID_ARGUMENT);
    CHECK(etl_encode(&frame, NULL, sizeof encoded, &written) ==
           ETL_INVALID_ARGUMENT);
    CHECK(etl_encode(&frame, encoded, sizeof encoded, NULL) ==
           ETL_INVALID_ARGUMENT);
    CHECK(etl_decoder_push(NULL, 0u, &frame) == ETL_INVALID_ARGUMENT);
    CHECK(etl_decoder_push(&decoder, 0u, NULL) == ETL_INVALID_ARGUMENT);
}

int main(void) {
    round_trip_preserves_frame();
    pico_demo_frame_round_trips_as_expected();
    reserved_bytes_are_escaped();
    fragmented_input_waits_for_the_closing_delimiter();
    crc_corruption_is_rejected();
    decoder_recovers_after_noise_and_corruption();
    payload_boundaries_round_trip();
    encoder_rejects_invalid_sizes();
    malformed_stream_input_is_rejected();
    public_functions_reject_null_pointers();
    puts("10 protocol tests passed");
    return 0;
}
