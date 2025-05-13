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

#define BUF_SIZE (1024)

#define PING_TIMEOUT 6000
#define ROUNDTRIP_M 5800.0f
#define ROUNDTRIP_CM 58
/* 
    DEFINES.
*/
static const char *TAG = "AJ-SR04M-SENSOR";
static level_sensor_cfg_t globalConfig;

esp_err_t InitLevelSensor(level_sensor_cfg_t *config) {
    globalConfig = *config;

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, TRIGGER_GPIO, ECHO_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "Sensor initialized");

    return gpio_set_level(TRIGGER_GPIO, 0);
}

esp_err_t UpdateLevelSensorConfig(level_sensor_cfg_t *config) {
    ESP_LOGD(TAG, "Updating sensor level configuration");
    globalConfig = *config;
    return ESP_OK;
}

void compute_other_values(level_sensor_val_t *level_values) {
    level_values->total_volume = globalConfig.total_volume * level_values->depth / globalConfig.max_depth;
    
    level_values->storage_volume = level_values->total_volume >= globalConfig.storage_volume ?
                 globalConfig.storage_volume : level_values->total_volume;

    level_values->retention_volume = level_values->total_volume > globalConfig.storage_volume ?
                level_values->total_volume - globalConfig.storage_volume : 0;

    ESP_LOGD(TAG, "Calculated values : total volume: %d, storage volume: %d, retention volume %d ", 
        level_values->total_volume, level_values->storage_volume, level_values->retention_volume);
 }

esp_err_t GetLevelSensorValue(level_sensor_val_t *values) {
    esp_err_t returnValue = ESP_ERR_NOT_FINISHED;
    ESP_LOGI(TAG, "Sensor get value started");

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    uint16_t distance = 0;

    // Trigger
    uint8_t triggerCmd = 1;

    uart_write_bytes_with_break(ECHO_UART_PORT_NUM, (const char *) &triggerCmd, 1, 100);

    // Reading
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(ECHO_UART_PORT_NUM, (size_t*)&length));

    ESP_LOGD(TAG, "Reading from UART");

    // Read data from the UART
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
    ESP_LOGD(TAG, "Reading length: %d", len);
    
    if (len && data[0] == 0xff) {

        distance = (data[1] << 8) + data[2];
        char  cs = data[0] + data[1] + data[2];

        if(cs != data[3]) {
            ESP_LOGD(TAG, "Invalid result !");
        } else {            
            ESP_LOGD(TAG, "Distance : %d mm", distance);
            values->depth = globalConfig.max_depth - roundf(distance / 10);
            compute_other_values(values);
            returnValue = ESP_OK;
        }
    }

    free(data);
    ESP_LOGD(TAG, "Ending calculation");
    
    return returnValue;
}
