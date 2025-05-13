#include "zigbee.h"
#include "sensor_driver.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zcl/esp_zigbee_zcl_common.h"

static const char *TAG = "ESP_HA_LIQUID_LEVEL_SENSOR";

static void esp_app_level_sensor_handler(level_sensor_val_t sensor_value)
{
    // int16_t measured_value = zb_temperature_to_s16(temperature);
    /* Update temperature sensor measured value */
    esp_zb_lock_acquire(portMAX_DELAY);
    uint16_t value = sensor_value.depth;
    ESP_LOGI(TAG, "Sensor app callback call: %d", value);
    
    esp_zb_zcl_set_manufacturer_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        LIQUID_LEVEL_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_MANUFACTURER_CODE,
        ATTR_CURRENT_LEVEL_ATTRIBUTE_ID, 
        &value, 
        false);

    uint16_t currentVolume = sensor_value.total_volume;
    esp_zb_zcl_set_manufacturer_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        LIQUID_LEVEL_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_MANUFACTURER_CODE,
        ATTR_TANK_CURRENT_VOLUME_ID, 
        &currentVolume, 
        false);

    uint16_t currentStorageVolume = sensor_value.storage_volume;
    esp_zb_zcl_set_manufacturer_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        LIQUID_LEVEL_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_MANUFACTURER_CODE,
        ATTR_TANK_CURRENT_STORAGE_VOLUME_ID, 
        &currentStorageVolume, 
        false);            

    uint16_t currentRetentionVolume = sensor_value.retention_volume;
    esp_zb_zcl_set_manufacturer_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        LIQUID_LEVEL_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_MANUFACTURER_CODE,
        ATTR_TANK_CURRENT_RETENTION_VOLUME_ID, 
        &currentRetentionVolume, 
        false);
    esp_zb_lock_release();
}

static level_sensor_cfg_t GetLevelSensorConfig() {
    uint16_t max_depth_value = *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_MAX_TANK_LEVEL_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;
    uint16_t total_volume = *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TANK_VOLUME_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;
    uint16_t storage_volume = *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TANK_STORAGE_VOLUME_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;
    uint16_t retention_volume =  *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TANK_RETENTION_VOLUME_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;

    level_sensor_cfg_t sensor_config = {
        .max_depth = max_depth_value,
        .total_volume = total_volume,
        .storage_volume = storage_volume,
        .retention_volume = retention_volume,
    };

    return sensor_config;
}

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;

    level_sensor_cfg_t sensor_config = GetLevelSensorConfig();

    if (!is_inited) {
        ESP_RETURN_ON_ERROR(
            level_sensor_driver_init(
                &sensor_config, 
                ESP_LEVEL_SENSOR_UPDATE_INTERVAL, 
                esp_app_level_sensor_handler),
            TAG, 
            "Failed to initialize temperature sensor");
        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}

void reportAttribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
{
    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = clusterID,
        .attributeID = attributeID,
        .manuf_code = ESP_MANUFACTURER_CODE,
    };
    esp_zb_zcl_attr_t *value_r = esp_zb_zcl_get_attribute(endpoint, clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attributeID);
    memcpy(value_r->data_p, value, value_length);
    esp_zb_zcl_report_attr_cmd_req(&cmd);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
    esp_zb_zdo_signal_leave_indication_params_t *leave_ind_params = NULL;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network formation");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                esp_zb_bdb_open_network(180);
                ESP_LOGI(TAG, "Device rebooted");
                ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "Restart network formation (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering started");
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (!leave_ind_params->rejoin) {
            esp_zb_ieee_addr_t leave_addr;
            memcpy(leave_addr, leave_ind_params->device_addr, sizeof(esp_zb_ieee_addr_t));
            ESP_LOGI(TAG, "Zigbee Node is leaving network: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
                     leave_addr[7], leave_addr[6], leave_addr[5], leave_addr[4],
                     leave_addr[3], leave_addr[2], leave_addr[1], leave_addr[0]);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == HA_ESP_SENSOR_ENDPOINT) {
         if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT) {
            switch (message->attribute.id) {
                case (ATTR_MAX_TANK_LEVEL_ID):
                case (ATTR_MIN_TANK_LEVEL_ID):
                case (ATTR_TANK_VOLUME_ID):
                case (ATTR_TANK_STORAGE_VOLUME_ID):
                case (ATTR_TANK_RETENTION_VOLUME_ID):                    
                    level_sensor_cfg_t sensor_config = GetLevelSensorConfig();
                    UpdateLevelSensorConfig(&sensor_config);
                    break;
            }
         }
    }
    return ret;
}

static esp_err_t zb_default_action_handler(esp_zb_core_action_callback_id_t callback_id, const esp_zb_zcl_cmd_default_resp_message_t *message)
{
    switch (message->status_code) {
    case ESP_ZB_ZCL_STATUS_SUCCESS:
        ESP_LOGD(TAG, "Receive Zigbee action (0x%x) status: %s", callback_id, "Success"); 
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action (0x%x) status: %d", callback_id, message->status_code); 
        break;
    }
    return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ESP_LOGW(TAG, "Receive Zigbee attribute action(0x%x) callback", callback_id);
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        ESP_LOGW(TAG, "Receive Zigbee report attribute action(0x%x) callback", callback_id);
        break;
    default:
        ret = zb_default_action_handler(callback_id, (esp_zb_zcl_cmd_default_resp_message_t *)message);
        break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters)
{
   
    /* initialize Zigbee stack with Zigbee coordinator config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* basic cluster create with fully customized */
    uint16_t application_version = APPLICATION_VERSION;
    uint16_t stack_version = STACK_VERSION;
    uint16_t hw_version = HW_VERSION;
    uint16_t power_source = POWER_SOURCE;
    uint16_t zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;

    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &application_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hw_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, DATE_CODE);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, SW_BUILD_ID);

    // ------------------------------ Cluster IDENTIFY ------------------------------
    esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {
        .identify_time = 0,
    };
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&identify_cluster_cfg);

    // ------------------------------ Cluster MEASUREMENT ------------------------------
    esp_zb_attribute_list_t *esp_zb_measure_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT);
    uint16_t level_zero_value = ATTR_CURRENT_LEVEL_DEFAULT_VALUE;
    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_CURRENT_LEVEL_ATTRIBUTE_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, 
        &level_zero_value
    );

    uint16_t max_tank_level = ATTR_MAX_TANK_LEVEL;
    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_MAX_TANK_LEVEL_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, 
        &max_tank_level
    );    

    uint16_t min_tank_level = ATTR_MIN_TANK_LEVEL;
    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_MIN_TANK_LEVEL_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, 
        &min_tank_level
    );  

    uint16_t tank_volume = ATTR_TANK_VOLUME;
    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_TANK_VOLUME_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, 
        &tank_volume
    );

    uint16_t tank_storage_volume = ATTR_TANK_STORAGE_VOLUME;
    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_TANK_STORAGE_VOLUME_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, 
        &tank_storage_volume
    );  

    uint16_t tank_retention_volume = ATTR_TANK_RETENTION_VOLUME;
    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_TANK_RETENTION_VOLUME_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, 
        &tank_retention_volume
    );  

    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_TANK_CURRENT_VOLUME_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,  
        &level_zero_value
    );

    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_TANK_CURRENT_STORAGE_VOLUME_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, 
        &level_zero_value
    );  

    esp_zb_cluster_add_manufacturer_attr(
        esp_zb_measure_cluster,
        LIQUID_LEVEL_CLUSTER_ID,
        ATTR_TANK_CURRENT_RETENTION_VOLUME_ID, 
        ESP_MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, 
        &level_zero_value
    );

    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();    
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_measure_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 1
    };

    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config));
    ESP_ERROR_CHECK(esp_zb_device_register(esp_zb_ep_list));
    
    esp_zb_core_action_handler_register(zb_action_handler);

    /* Config the reporting info  */
    esp_zb_zcl_reporting_info_t reporting_info_current_level = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .attr_id = ATTR_CURRENT_LEVEL_ATTRIBUTE_ID,
        .manuf_code = ESP_MANUFACTURER_CODE,
    };

    ESP_ERROR_CHECK(esp_zb_zcl_update_reporting_info(&reporting_info_current_level));

    /* Config the reporting info  */
    esp_zb_zcl_reporting_info_t reporting_info_current_volume = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .attr_id = ATTR_TANK_CURRENT_VOLUME_ID,
        .manuf_code = ESP_MANUFACTURER_CODE,
    };

    ESP_ERROR_CHECK(esp_zb_zcl_update_reporting_info(&reporting_info_current_volume));

    /* Config the reporting info  */
    esp_zb_zcl_reporting_info_t reporting_info_current_storage_volume = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .attr_id = ATTR_TANK_CURRENT_STORAGE_VOLUME_ID,
        .manuf_code = ESP_MANUFACTURER_CODE,
    };

    ESP_ERROR_CHECK(esp_zb_zcl_update_reporting_info(&reporting_info_current_storage_volume));

    /* Config the reporting info  */
    esp_zb_zcl_reporting_info_t reporting_info_current_retention_volume = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .attr_id = ATTR_TANK_CURRENT_RETENTION_VOLUME_ID,
        .manuf_code = ESP_MANUFACTURER_CODE,
    };

    ESP_ERROR_CHECK(esp_zb_zcl_update_reporting_info(&reporting_info_current_retention_volume));

    ESP_ERROR_CHECK(esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void app_main(void)
{
    esp_log_level_set("AJ-SR04M-SENSOR", ESP_LOG_DEBUG);

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
    // ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
}