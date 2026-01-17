#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#define LED_PIN            GPIO_NUM_14
#define BIT_DURATION_MS    100
#define START_FRAME_MS     700
#define STOP_FRAME_MS      1000

#define UART_NUM           UART_NUM_0
#define BUF_SIZE           1024

static const char *TAG = "LIFI_TX";

/* ---------------- Li-Fi TX ---------------- */

void transmit_start_frame(void) {
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(START_FRAME_MS));
}

void transmit_stop_frame(void) {
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(STOP_FRAME_MS));
}

void transmit_bit(int bit) {
    gpio_set_level(LED_PIN, bit);
    vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS));
}

void transmit_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        transmit_bit((byte >> i) & 1);
    }
}

/* ---------------- UART ---------------- */

void uart_init(void) {
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &cfg);
    uart_driver_install(UART_NUM, BUF_SIZE, 0, 0, NULL, 0);
}

/* ---------------- MAIN ---------------- */

void app_main(void) {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    uart_init();

    ESP_LOGI(TAG, "Li-Fi TX ready. Waiting for encrypted byte from Python...");

    uint8_t enc_byte;

    while (1) {
        int len = uart_read_bytes(UART_NUM, &enc_byte, 1, portMAX_DELAY);
        if (len == 1) {
            ESP_LOGI(TAG, "Received encrypted byte: 0x%02X", enc_byte);

            transmit_start_frame();
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS));

            transmit_byte(enc_byte);

            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS));

            transmit_stop_frame();
            gpio_set_level(LED_PIN, 0);

            ESP_LOGI(TAG, "Transmission done");
        }
    }
}
