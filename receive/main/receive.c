#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// ADC channel for the LDR sensor (GPIO34)
#define LDR_ADC_CHANNEL       ADC_CHANNEL_6         // GPIO34
#define LIGHT_THRESHOLD       700                  // Above = light ON, below = light OFF
#define BIT_DURATION_MS       100
#define START_FRAME_MIN_MS    600                   // Allow some tolerance
#define START_FRAME_MAX_MS    800
#define STOP_FRAME_MIN_MS     900
#define STOP_FRAME_MAX_MS     1100
#define SAMPLE_INTERVAL_MS    10                    // Check sensor every 10ms

static const char *TAG = "LIFI_RX";
static adc_oneshot_unit_handle_t adc_handle;

/**
 * @brief Initializes the ADC for the LDR sensor on GPIO34.
 */
void adc_init(void) {
    // ADC1 unit configuration
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // ADC channel configuration
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12  // Use 12dB attenuation for full 0-3.3V range
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, LDR_ADC_CHANNEL, &config));
    
    ESP_LOGI(TAG, "ADC initialized for light sensor on GPIO34");
}

/**
 * @brief Reads the raw ADC value from the LDR.
 * @return The raw ADC value.
 */
uint32_t read_light_sensor(void) {
    int adc_reading = 0;
    
    // Average multiple samples for stability
    for (int i = 0; i < 10; i++) {
        int value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, LDR_ADC_CHANNEL, &value));
        adc_reading += value;
    }
    adc_reading /= 10;
    
    return (uint32_t)adc_reading;
}

/**
 * @brief Check if light is ON (above threshold).
 */
bool is_light_on(void) {
    return read_light_sensor() > LIGHT_THRESHOLD;
}

/**
 * @brief Wait for light to be in specific state with timeout.
 */
bool wait_for_light_state(bool expected_state, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (is_light_on() == expected_state) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
        elapsed += SAMPLE_INTERVAL_MS;
    }
    return false;
}

/**
 * @brief Measure how long light stays in current state.
 */
uint32_t measure_light_duration(bool state) {
    uint32_t duration = 0;
    while (is_light_on() == state) {
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
        duration += SAMPLE_INTERVAL_MS;
        
        // Safety timeout to prevent infinite loop
        if (duration > 2000) break;
    }
    return duration;
}

/**
 * @brief Detect START frame (700ms LED ON).
 */
bool detect_start_frame(void) {
    ESP_LOGI(TAG, "Waiting for START frame...");
    
    // Wait for light to turn ON
    if (!wait_for_light_state(true, 10000)) {
        ESP_LOGW(TAG, "Timeout waiting for light ON");
        return false;
    }
    
    // Measure how long light stays ON
    uint32_t on_duration = measure_light_duration(true);
    
    ESP_LOGI(TAG, "Light ON duration: %lu ms", on_duration);
    
    if (on_duration >= START_FRAME_MIN_MS && on_duration <= START_FRAME_MAX_MS) {
        ESP_LOGI(TAG, "START frame detected!");
        return true;
    }
    
    return false;
}

/**
 * @brief Detect STOP frame (1000ms LED ON).
 */
bool detect_stop_frame(void) {
    ESP_LOGI(TAG, "Waiting for STOP frame...");
    
    // Wait for light to turn ON
    if (!wait_for_light_state(true, 2000)) {
        ESP_LOGW(TAG, "Timeout waiting for STOP frame");
        return false;
    }
    
    // Measure how long light stays ON
    uint32_t on_duration = measure_light_duration(true);
    
    ESP_LOGI(TAG, "Light ON duration: %lu ms", on_duration);
    
    if (on_duration >= STOP_FRAME_MIN_MS && on_duration <= STOP_FRAME_MAX_MS) {
        ESP_LOGI(TAG, "STOP frame detected!");
        return true;
    }
    
    return false;
}

/**
 * @brief Read a single bit.
 */
int read_bit(void) {
    // Wait for bit to stabilize
    vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS / 2));
    
    // Sample in the middle of bit duration
    bool bit_value = is_light_on();
    uint32_t light_value = read_light_sensor();
    
    ESP_LOGI(TAG, "  Bit: %d (sensor: %lu)", bit_value ? 1 : 0, light_value);
    
    // Wait for rest of bit duration
    vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS / 2));
    
    return bit_value ? 1 : 0;
}

/**
 * @brief Read 8 bits to form a byte.
 */
uint8_t read_byte(void) {
    uint8_t byte = 0;
    
    ESP_LOGI(TAG, "Reading 8 bits...");
    
    for (int i = 7; i >= 0; i--) {
        int bit = read_bit();
        byte |= (bit << i);
    }
    
    ESP_LOGI(TAG, "Received byte: 0x%02X (%d)", byte, byte);
    return byte;
}

/**
 * @brief Main application entry point.
 */

void app_main(void) {
    adc_init();

    ESP_LOGI(TAG, "Li-Fi RX ready. Sending received bytes to Python.");

    while (1) {

        if (detect_start_frame()) {

            vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS));
            uint8_t received_byte = read_byte();
            vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS));

            if (detect_stop_frame()) {
                ESP_LOGI(TAG, "Received encrypted byte: 0x%02X", received_byte);

                // Send to Python
                printf("%02X\n", received_byte);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
