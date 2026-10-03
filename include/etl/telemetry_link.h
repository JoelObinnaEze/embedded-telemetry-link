#ifndef ETL_TELEMETRY_LINK_H
#define ETL_TELEMETRY_LINK_H

#include <stddef.h>
#include <stdint.h>

#define ETL_PROTOCOL_VERSION 1u
#define ETL_MAX_PAYLOAD_SIZE 64u
#define ETL_MAX_BODY_SIZE (4u + ETL_MAX_PAYLOAD_SIZE + 2u)
#define ETL_MAX_ENCODED_SIZE (2u + (2u * ETL_MAX_BODY_SIZE))

typedef enum {
    ETL_OK = 0,
    ETL_FRAME_READY,
    ETL_INVALID_ARGUMENT,
    ETL_BUFFER_TOO_SMALL,
    ETL_PAYLOAD_TOO_LARGE,
    ETL_FRAME_REJECTED
} etl_status_t;

typedef struct {
    uint8_t version;
    uint8_t message_type;
    uint8_t sequence;
    uint8_t payload_length;
    uint8_t payload[ETL_MAX_PAYLOAD_SIZE];
} etl_frame_t;

typedef struct {
    uint8_t buffer[ETL_MAX_BODY_SIZE];
    size_t length;
    uint8_t active;
    uint8_t escaping;
} etl_decoder_t;

#define ETL_DECODER_INIT {{0}, 0u, 0u, 0u}

etl_status_t etl_encode(const etl_frame_t *frame,
                        uint8_t *output,
                        size_t capacity,
                        size_t *written);

etl_status_t etl_decoder_push(etl_decoder_t *decoder,
                              uint8_t byte,
                              etl_frame_t *frame);

#endif
