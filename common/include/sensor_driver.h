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

 #pragma once

 #include "level_sensor.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /** Temperature sensor callback
  *
  * @param[in] temperature temperature value in degrees Celsius from sensor
  *
  */
 typedef void (*esp_level_sensor_callback_t)(level_sensor_val_t values);
 
 /**
  * @brief init function for temp sensor and callback setup
  *
  * @param config                pointer of temperature sensor config.
  * @param update_interval       sensor value update interval in seconds.
  * @param cb                    callback pointer.
  *
  * @return ESP_OK if the driver initialization succeed, otherwise ESP_FAIL.
  */
 esp_err_t level_sensor_driver_init(level_sensor_cfg_t *config, uint16_t update_interval, esp_level_sensor_callback_t cb);

 esp_err_t update_sensor_config(level_sensor_cfg_t *config);
 
 #ifdef __cplusplus
 } // extern "C"
 #endif