#include "esp_err.h"

#define MAX_DISTANCE_CM 450

#define TRIGGER_GPIO 10
#define ECHO_GPIO 11

#define ESP_ERR_ULTRASONIC_PING         0x200
#define ESP_ERR_ULTRASONIC_PING_TIMEOUT 0x201
#define ESP_ERR_ULTRASONIC_ECHO_TIMEOUT 0x202

typedef struct level_sensor_cfg_s {
    int max_depth;
    int min_depth;
    int total_volume;
    int storage_volume;
    int retention_volume;
} level_sensor_cfg_t;

typedef struct level_sensor_val_s {
    int depth;
    int total_volume;
    int total_volume_percentage;
    int storage_volume;
    int retention_volume;
} level_sensor_val_t;

esp_err_t InitLevelSensor(level_sensor_cfg_t *config);

esp_err_t UpdateLevelSensorConfig(level_sensor_cfg_t *config);

esp_err_t GetLevelSensorValue(level_sensor_val_t *values);