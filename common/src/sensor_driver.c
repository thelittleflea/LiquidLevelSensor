/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee temperature sensor driver example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

 #include "sensor_driver.h"
 
 #include "esp_err.h"
 #include "esp_log.h"
 #include "esp_check.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 
 /**
  * @brief:
  * This example code shows how to configure temperature sensor.
  *
  * @note:
  * The callback will be called with updated temperature sensor value every $interval seconds.
  *
  */ 

 /* call back function pointer */
 static esp_level_sensor_callback_t func_ptr;
 /* update interval in seconds */
 static uint16_t interval = 15;
 
 static const char *TAG = "ESP_SENSOR_DRIVER";
 
 /**
  * @brief Tasks for updating the sensor value
  *
  * @param arg      Unused value.
  */
 static void level_sensor_driver_value_update(void *arg)
 {
    esp_err_t returnValue;
     while (1) {
        ESP_LOGD(TAG, "Run loop.");
         level_sensor_val_t level_values;
         returnValue = GetLevelSensorValue(&level_values);
         if (returnValue == ESP_OK && func_ptr) {
             func_ptr(level_values);
         } else {
            ESP_LOGW(TAG, "No level value to update .....");
         }
         ESP_LOGD(TAG, "Loop wait .....");
         vTaskDelay(pdMS_TO_TICKS(interval * 1000));
         ESP_LOGD(TAG, "Loop end.");
     }
 }
 
 /**
  * @brief init temperature sensor
  *
  * @param config      pointer of temperature sensor config.
  */
 static esp_err_t sensor_driver_sensor_init(level_sensor_cfg_t *config)
 {
     ESP_RETURN_ON_ERROR(InitLevelSensor(config),
                         TAG, "Fail to configure level sensor");
     return (xTaskCreate(level_sensor_driver_value_update, "level_sensor_update", 4096, NULL, 10, NULL) == pdTRUE) ? ESP_OK : ESP_FAIL;
 }
 
 esp_err_t level_sensor_driver_init(level_sensor_cfg_t *config, uint16_t update_interval, esp_level_sensor_callback_t cb)
 {
     if (ESP_OK != sensor_driver_sensor_init(config)) {
         return ESP_FAIL;
     }
     func_ptr = cb;
     interval = update_interval;
     return ESP_OK;
 } 

 esp_err_t update_sensor_config(level_sensor_cfg_t *config) {
    UpdateLevelSensorConfig(config);
    return ESP_OK;
 }