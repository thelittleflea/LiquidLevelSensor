#include "level_sensor.h"
#include "math.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include "esp_log.h"
#include <ets_sys.h>
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>

#define TRIGGER_LOW_DELAY       4
#define TRIGGER_HIGH_DELAY      12
#define ECHO_UART_BAUD_RATE     9600
#define ECHO_TASK_STACK_SIZE    128
#define ECHO_UART_PORT_NUM      UART_NUM_1

#define BUF_SIZE (256)

#define PING_TIMEOUT 6000
#define ROUNDTRIP_M 5800.0f
#define ROUNDTRIP_CM 58
/* 
    DEFINES.
*/
static const char *TAG = "A02YYUW-SENSOR";
static level_sensor_cfg_t globalConfig;

esp_err_t InitLevelSensor(level_sensor_cfg_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    globalConfig = *config;

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, TRIGGER_GPIO, ECHO_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, 256, 256, 0, NULL, 0));

    ESP_LOGI(TAG, "Sensor initialized");

    return gpio_set_level(TRIGGER_GPIO, 0);
}

esp_err_t UpdateLevelSensorConfig(level_sensor_cfg_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "Updating sensor level configuration");
    globalConfig = *config;
    return ESP_OK;
}

void compute_other_values(level_sensor_val_t *level_values) {
    if (level_values == NULL) {
        ESP_LOGE(TAG, "Level values pointer is NULL");
        return;
    }
    if (globalConfig.max_tank_depth == 0) {
        ESP_LOGE(TAG, "Max tank depth is zero, cannot compute volumes");
        return;
    }
    level_values->total_volume = globalConfig.total_volume * level_values->depth / globalConfig.max_tank_depth;
    
    // Calculate total_volume_percentage
    level_values->total_volume_percentage = (level_values->depth * 100) / globalConfig.max_tank_depth;
    
    level_values->storage_volume = level_values->total_volume >= globalConfig.storage_volume ?
                 globalConfig.storage_volume : level_values->total_volume;

    level_values->retention_volume = level_values->total_volume > globalConfig.storage_volume ?
                level_values->total_volume - globalConfig.storage_volume : 0;

    ESP_LOGD(TAG, "Calculated values : depth: %d cm, total volume: %d L (%d%%), storage volume: %d L, retention volume: %d L", 
        level_values->depth, level_values->total_volume, level_values->total_volume_percentage, level_values->storage_volume, level_values->retention_volume);
 }

esp_err_t GetLevelSensorValue(level_sensor_val_t *values) {
    if (values == NULL) {
        ESP_LOGE(TAG, "Values pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize values to avoid garbage data
    memset(values, 0, sizeof(level_sensor_val_t));
    
    esp_err_t returnValue = ESP_FAIL;
    ESP_LOGI(TAG, "Sensor get value started");

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    uint16_t distance = 0;

    // Trigger
    uint8_t triggerCmd = 1;

    uart_write_bytes_with_break(ECHO_UART_PORT_NUM, (const char *) &triggerCmd, 1, 100);

    // Reading
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(ECHO_UART_PORT_NUM, (size_t*)&length));

    ESP_LOGD(TAG, "Reading from UART");

    // Read data from the UART with longer timeout
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 200 / portTICK_PERIOD_MS);
    ESP_LOGD(TAG, "Reading length: %d", len);
    
    if (len >= 4 && data[0] == 0xff) {

        distance = (data[1] << 8) + data[2];
        uint8_t cs = data[0] + data[1] + data[2];

        if(cs != data[3]) {
            ESP_LOGD(TAG, "Invalid result !");
        } else {            
            ESP_LOGD(TAG, "Distance : %d mm", distance);
            values->depth = (globalConfig.max_tank_depth + globalConfig.sensor_offset_height) - roundf(distance / 10);
            
            // Clamp depth between 0 and max_tank_depth
            if (values->depth < 0) {
                values->depth = 0;
                ESP_LOGW(TAG, "Depth below minimum, clamped to 0");
            } else if (values->depth > globalConfig.max_tank_depth) {
                values->depth = globalConfig.max_tank_depth;
                ESP_LOGW(TAG, "Depth above maximum, clamped to %d", globalConfig.max_tank_depth);
            }
            
            compute_other_values(values);
            returnValue = ESP_OK;
        }
    }

    free(data);
    ESP_LOGD(TAG, "Ending calculation");
    
    return returnValue;
}
