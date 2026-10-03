#include "etl/telemetry_link.h"

#include <string.h>

#define ETL_DELIMITER 0x7eu
#define ETL_ESCAPE 0x7du
#define ETL_ESCAPE_XOR 0x20u
#define ETL_HEADER_SIZE 4u
#define ETL_CRC_SIZE 2u

static uint16_t crc16_ccitt_false(const uint8_t *data, size_t length) {
    uint16_t crc = 0xffffu;

    for (size_t i = 0u; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8u;
        for (unsigned bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u
                      ? (uint16_t)((crc << 1u) ^ 0x1021u)
                      : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}

static int is_reserved(uint8_t byte) {
    return byte == ETL_DELIMITER || byte == ETL_ESCAPE;
}

etl_status_t etl_encode(const etl_frame_t *frame,
                        uint8_t *output,
                        size_t capacity,
                        size_t *written) {
    uint8_t body[ETL_MAX_BODY_SIZE];
    size_t body_length;
    size_t needed = 2u;
    size_t cursor = 0u;
    uint16_t crc;

    if (frame == NULL || output == NULL || written == NULL) {
        return ETL_INVALID_ARGUMENT;
    }
    *written = 0u;
    if (frame->payload_length > ETL_MAX_PAYLOAD_SIZE) {
        return ETL_PAYLOAD_TOO_LARGE;
    }

    body[0] = frame->version;
    body[1] = frame->message_type;
    body[2] = frame->sequence;
    body[3] = frame->payload_length;
    memcpy(&body[ETL_HEADER_SIZE], frame->payload, frame->payload_length);
    body_length = ETL_HEADER_SIZE + frame->payload_length + ETL_CRC_SIZE;
    crc = crc16_ccitt_false(body, body_length - ETL_CRC_SIZE);
    body[body_length - 2u] = (uint8_t)(crc >> 8u);
    body[body_length - 1u] = (uint8_t)crc;

    for (size_t i = 0u; i < body_length; ++i) {
        needed += is_reserved(body[i]) ? 2u : 1u;
    }
    if (capacity < needed) {
        return ETL_BUFFER_TOO_SMALL;
    }

    output[cursor++] = ETL_DELIMITER;
    for (size_t i = 0u; i < body_length; ++i) {
        if (is_reserved(body[i])) {
            output[cursor++] = ETL_ESCAPE;
            output[cursor++] = body[i] ^ ETL_ESCAPE_XOR;
        } else {
            output[cursor++] = body[i];
        }
    }
    output[cursor++] = ETL_DELIMITER;
    *written = cursor;
    return ETL_OK;
}

static void reset_decoder(etl_decoder_t *decoder, uint8_t active) {
    decoder->length = 0u;
    decoder->active = active;
    decoder->escaping = 0u;
}

static etl_status_t finish_frame(etl_decoder_t *decoder, etl_frame_t *frame) {
    size_t content_length;
    uint8_t payload_length;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if (decoder->length < ETL_HEADER_SIZE + ETL_CRC_SIZE) {
        return ETL_FRAME_REJECTED;
    }
    payload_length = decoder->buffer[3];
    content_length = ETL_HEADER_SIZE + payload_length;
    if (payload_length > ETL_MAX_PAYLOAD_SIZE ||
        decoder->length != content_length + ETL_CRC_SIZE ||
        decoder->buffer[0] != ETL_PROTOCOL_VERSION) {
        return ETL_FRAME_REJECTED;
    }

    expected_crc = (uint16_t)((uint16_t)decoder->buffer[content_length] << 8u) |
                   decoder->buffer[content_length + 1u];
    actual_crc = crc16_ccitt_false(decoder->buffer, content_length);
    if (actual_crc != expected_crc) {
        return ETL_FRAME_REJECTED;
    }

    frame->version = decoder->buffer[0];
    frame->message_type = decoder->buffer[1];
    frame->sequence = decoder->buffer[2];
    frame->payload_length = payload_length;
    memcpy(frame->payload, &decoder->buffer[ETL_HEADER_SIZE], payload_length);
    return ETL_FRAME_READY;
}

etl_status_t etl_decoder_push(etl_decoder_t *decoder,
                              uint8_t byte,
                              etl_frame_t *frame) {
    etl_status_t status;

    if (decoder == NULL || frame == NULL) {
        return ETL_INVALID_ARGUMENT;
    }
    if (byte == ETL_DELIMITER) {
        if (decoder->active == 0u) {
            reset_decoder(decoder, 1u);
            return ETL_OK;
        }
        if (decoder->escaping != 0u) {
            reset_decoder(decoder, 1u);
            return ETL_FRAME_REJECTED;
        }
        if (decoder->length == 0u) {
            return ETL_OK;
        }
        status = finish_frame(decoder, frame);
        reset_decoder(decoder, 1u);
        return status;
    }
    if (decoder->active == 0u) {
        return ETL_OK;
    }
    if (decoder->escaping != 0u) {
        decoder->escaping = 0u;
        if (byte != (ETL_DELIMITER ^ ETL_ESCAPE_XOR) &&
            byte != (ETL_ESCAPE ^ ETL_ESCAPE_XOR)) {
            reset_decoder(decoder, 0u);
            return ETL_FRAME_REJECTED;
        }
        byte ^= ETL_ESCAPE_XOR;
    } else if (byte == ETL_ESCAPE) {
        decoder->escaping = 1u;
        return ETL_OK;
    }

    if (decoder->length >= ETL_MAX_BODY_SIZE) {
        reset_decoder(decoder, 0u);
        return ETL_FRAME_REJECTED;
    }
    decoder->buffer[decoder->length++] = byte;
    if (decoder->length >= ETL_HEADER_SIZE) {
        const size_t expected = ETL_HEADER_SIZE + decoder->buffer[3] + ETL_CRC_SIZE;
        if (decoder->buffer[3] > ETL_MAX_PAYLOAD_SIZE || decoder->length > expected) {
            reset_decoder(decoder, 0u);
            return ETL_FRAME_REJECTED;
        }
    }
    return ETL_OK;
}
