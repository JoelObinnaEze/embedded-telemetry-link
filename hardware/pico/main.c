#include "etl/telemetry_link.h"

#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include <stdio.h>
#include <string.h>

#define UART_ID uart0
#define UART_IRQ UART0_IRQ
#define UART_TX_PIN 0u
#define UART_RX_PIN 1u
#define UART_BAUD_RATE 115200u
#define RESULT_TIMEOUT_MS 1000u

static etl_decoder_t decoder = ETL_DECODER_INIT;
static etl_frame_t received_frame;
static volatile etl_status_t receive_status = ETL_OK;
static volatile uint8_t received_sequence;
static volatile uint32_t received_byte_count;
static volatile uint8_t received_bytes[ETL_MAX_ENCODED_SIZE];

static void on_uart_rx(void) {
    while (uart_is_readable(UART_ID)) {
        const uint8_t byte = uart_getc(UART_ID);
        received_byte_count++;
        if (received_byte_count <= sizeof received_bytes) {
            received_bytes[received_byte_count - 1u] = byte;
        }
        const etl_status_t status =
            etl_decoder_push(&decoder, byte, &received_frame);
        if (status == ETL_FRAME_READY || status == ETL_FRAME_REJECTED) {
            if (status == ETL_FRAME_READY) {
                received_sequence = received_frame.sequence;
            }
            receive_status = status;
        }
    }
}

static bool wait_for_status(etl_status_t expected) {
    const absolute_time_t deadline = make_timeout_time_ms(RESULT_TIMEOUT_MS);

    while (receive_status != expected) {
        if (time_reached(deadline)) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
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

    stdio_init_all();
    sleep_ms(2000u);

    /* Keep RX idle-high before enabling the UART to avoid a false start bit. */
    gpio_pull_up(UART_RX_PIN);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_init(UART_ID, UART_BAUD_RATE);
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);

    if (etl_encode(&sensor_frame, encoded, sizeof encoded, &written) != ETL_OK) {
        puts("FAIL: encoding");
        return 1;
    }

    receive_status = ETL_OK;
    received_byte_count = 0u;
    memset((void *)received_bytes, 0, sizeof received_bytes);
    uart_write_blocking(UART_ID, encoded, written);
    if (!wait_for_status(ETL_FRAME_READY)) {
        if (received_byte_count == 0u) {
            puts("FAIL: UART0 transmitted, but received 0 bytes; check GP0-to-GP1 wiring");
        } else {
            printf("FAIL: UART0 received %lu bytes, but no valid frame was decoded\n",
                   (unsigned long)received_byte_count);
            printf("RX:");
            const size_t captured =
                received_byte_count < sizeof received_bytes
                    ? received_byte_count
                    : sizeof received_bytes;
            for (size_t i = 0u; i < captured; ++i) {
                printf(" %02X", received_bytes[i]);
            }
            puts("");
        }
        return 1;
    }
    if (received_sequence != sensor_frame.sequence) {
        puts("FAIL: decoded sequence did not match");
        return 1;
    }
    printf("PASS: UART IRQ decoded sequence %u\n", (unsigned)received_sequence);

    memcpy(corrupted, encoded, written);
    corrupted[2] ^= 0x01u;
    receive_status = ETL_OK;
    uart_write_blocking(UART_ID, corrupted, written);
    if (!wait_for_status(ETL_FRAME_REJECTED)) {
        puts("FAIL: corrupted frame was not rejected");
        return 1;
    }
    puts("PASS: CRC corruption rejected");

    receive_status = ETL_OK;
    uart_write_blocking(UART_ID, noise, sizeof noise);
    uart_write_blocking(UART_ID, encoded, written);
    if (!wait_for_status(ETL_FRAME_READY)) {
        puts("FAIL: decoder did not recover after noise");
        return 1;
    }
    puts("PASS: decoder recovered after noise");
    puts("HARDWARE DEMO PASSED");

    while (true) {
        tight_loop_contents();
    }
}
