#include "zigbee.h"
#include "sensor_driver.h"

#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_app_desc.h"
#include "array.h"

static const char *TAG = "ESP_HA_LIQUID_LEVEL_SENSOR";

static const esp_partition_t *s_ota_partition = NULL;
static esp_ota_handle_t s_ota_handle = 0;
static bool s_tagid_received = false;

#define OTA_UPGRADE_QUERY_INTERVAL (1 * 60) // 1 minutes


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
    uint16_t min_depth_value = *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_MIN_TANK_LEVEL_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;
    uint16_t total_volume = *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TANK_VOLUME_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;
    uint16_t storage_volume = *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TANK_STORAGE_VOLUME_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;
    uint16_t retention_volume =  *(uint16_t *) esp_zb_zcl_get_manufacturer_attribute(HA_ESP_SENSOR_ENDPOINT, LIQUID_LEVEL_CLUSTER_ID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TANK_RETENTION_VOLUME_ID, ESP_MANUFACTURER_CODE)
                                        ->data_p;

    level_sensor_cfg_t sensor_config = {
        .max_depth = max_depth_value,
        .min_depth = min_depth_value,
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

static esp_err_t esp_element_ota_data(uint32_t total_size, const void *payload, uint16_t payload_size, void **outbuf, uint16_t *outlen)
{
    static uint16_t tagid = 0;
    void *data_buf = NULL;
    uint16_t data_len;

    if (!s_tagid_received) {
        uint32_t length = 0;
        if (!payload || payload_size <= OTA_ELEMENT_HEADER_LEN) {
            ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Invalid element format");
        }

        tagid  = *(const uint16_t *)payload;
        length = *(const uint32_t *)(payload + sizeof(tagid));
        if ((length + OTA_ELEMENT_HEADER_LEN) != total_size) {
            ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Invalid element length [%ld/%ld]", length, total_size);
        }

        s_tagid_received = true;

        data_buf = (void *)(payload + OTA_ELEMENT_HEADER_LEN);
        data_len = payload_size - OTA_ELEMENT_HEADER_LEN;
    } else {
        data_buf = (void *)payload;
        data_len = payload_size;
    }

    switch (tagid) {
        case UPGRADE_IMAGE:
            *outbuf = data_buf;
            *outlen = data_len;
            break;
        default:
            ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Unsupported element tag identifier %d", tagid);
            break;
    }

    return ESP_OK;
}

static esp_err_t zb_ota_upgrade_status_handler(esp_zb_zcl_ota_upgrade_value_message_t message)
{
    static uint32_t total_size = 0;
    static uint32_t offset = 0;
    static int64_t start_time = 0;
    esp_err_t ret = ESP_OK;

    if (message.info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        switch (message.upgrade_status) {
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
            ESP_LOGI(TAG, "-- OTA upgrade start");
            start_time = esp_timer_get_time();
            s_ota_partition = esp_ota_get_next_update_partition(NULL);
            assert(s_ota_partition);
            
            ret = esp_ota_begin(s_ota_partition, 0, &s_ota_handle);
            
            ESP_RETURN_ON_ERROR(ret, TAG, "Failed to begin OTA partition, status: %s", esp_err_to_name(ret));
            break;
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
            total_size = message.ota_header.image_size;
            offset += message.payload_size;
            ESP_LOGI(TAG, "-- OTA Client receives data: progress [%ld/%ld]", offset, total_size);
            if (message.payload_size && message.payload) {
                uint16_t payload_size = 0;
                void    *payload = NULL;
                ret = esp_element_ota_data(total_size, message.payload, message.payload_size, &payload, &payload_size);
                ESP_RETURN_ON_ERROR(ret, TAG, "Failed to element OTA data, status: %s", esp_err_to_name(ret));

                ret = esp_ota_write(s_ota_handle, (const void *)payload, payload_size);

                ESP_RETURN_ON_ERROR(ret, TAG, "Failed to write OTA data to partition, status: %s", esp_err_to_name(ret));
            }
            break;
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY:
            ESP_LOGI(TAG, "-- OTA upgrade apply");
            break;
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
            ret = offset == total_size ? ESP_OK : ESP_FAIL;
            offset = 0;
            total_size = 0;
            s_tagid_received = false;
            ESP_LOGI(TAG, "-- OTA upgrade check status: %s", esp_err_to_name(ret));
            break;
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
            ESP_LOGI(TAG, "-- OTA Finish");
            ESP_LOGI(TAG, "-- OTA Information: version: 0x%lx, manufacturer code: 0x%x, image type: 0x%x, total size: %ld bytes, cost time: %lld ms,",
                     message.ota_header.file_version, message.ota_header.manufacturer_code, message.ota_header.image_type,
                     message.ota_header.image_size, (esp_timer_get_time() - start_time) / 1000);

            ret = esp_ota_end(s_ota_handle);

            ESP_RETURN_ON_ERROR(ret, TAG, "Failed to end OTA partition, status: %s", esp_err_to_name(ret));
            ret = esp_ota_set_boot_partition(s_ota_partition);
            ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set OTA boot partition, status: %s", esp_err_to_name(ret));
            ESP_LOGW(TAG, "Prepare to restart system");
            esp_restart();
            break;
        default:
            ESP_LOGI(TAG, "OTA status: %d", message.upgrade_status);
            break;
        }
    }
    return ret;
}

static esp_err_t zb_ota_upgrade_query_image_resp_handler(esp_zb_zcl_ota_upgrade_query_image_resp_message_t message)
{
    esp_err_t ret = ESP_OK;
    if (message.info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Queried OTA image from address: 0x%04hx, endpoint: %d", message.server_addr.u.short_addr, message.server_endpoint);
        ESP_LOGI(TAG, "Image version: 0x%lx, manufacturer code: 0x%x, image size: %ld", message.file_version, message.manufacturer_code,
                 message.image_size);
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Approving OTA image upgrade");
    } else {
        ESP_LOGI(TAG, "Rejecting OTA image upgrade, status: %s", esp_err_to_name(ret));
    }
    return ret;
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
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
        ret = zb_ota_upgrade_status_handler(*(esp_zb_zcl_ota_upgrade_value_message_t *)message);
        break;
    case ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID:
        ret = zb_ota_upgrade_query_image_resp_handler(*(esp_zb_zcl_ota_upgrade_query_image_resp_message_t *)message);
        break;
    default:
        ret = zb_default_action_handler(callback_id, (esp_zb_zcl_cmd_default_resp_message_t *)message);
        break;
    }
    return ret;
}

static esp_zb_attribute_list_t *create_zb_measurement_cluter() 
{
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
    return esp_zb_measure_cluster;
}

static esp_zb_attribute_list_t *create_zb_ota_cluter() 
{
    /** Create ota client cluster with attributes.
     *  Manufacturer code, image type and file version should match with configured values for server.
     *  If the client values do not match with configured values then it shall discard the command and
     *  no further processing shall continue.
     */
    esp_zb_ota_cluster_cfg_t ota_cluster_cfg = {
        .ota_upgrade_file_version = OTA_IMAGE_FILE_VERSION,
        .ota_upgrade_downloaded_file_ver = OTA_UPGRADE_DOWNLOADED_FILE_VERSION,
        .ota_upgrade_manufacturer = ESP_MANUFACTURER_CODE,
        .ota_upgrade_image_type = OTA_UPGRADE_IMAGE_TYPE,
    };
    esp_zb_attribute_list_t *esp_zb_ota_cluster = esp_zb_ota_cluster_create(&ota_cluster_cfg);

    esp_zb_zcl_ota_upgrade_client_variable_t variable_config = {
        .timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF,
        .hw_version = HW_VERSION,
        .max_data_size = OTA_UPGRADE_MAX_DATA_SIZE,
    };

    uint16_t ota_upgrade_server_addr = 0xffff;
    uint8_t ota_upgrade_server_ep = 0xff;    

    ESP_ERROR_CHECK(esp_zb_ota_cluster_add_attr(esp_zb_ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, (void *)&variable_config));
    ESP_ERROR_CHECK(esp_zb_ota_cluster_add_attr(esp_zb_ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, (void *)&ota_upgrade_server_addr));
    ESP_ERROR_CHECK(esp_zb_ota_cluster_add_attr(esp_zb_ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, (void *)&ota_upgrade_server_ep));

    return esp_zb_ota_cluster;
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
    char version[9] = {0};
    char buildDate[9] = {0};
    strCopyWithSize(version, OTA_IMAGE_VERSION);
    strCopyWithSize(buildDate, OTA_IMAGE_FILE_DATE);

    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &application_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hw_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, &buildDate);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, &version);

    // ------------------------------ Cluster IDENTIFY ------------------------------
    esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {
        .identify_time = 0,
    };
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&identify_cluster_cfg);

    // ------------------------------ Cluster MEASUREMENT ---------------------------
    esp_zb_attribute_list_t *esp_zb_measure_cluster = create_zb_measurement_cluter();

    // ------------------------------ Cluster OTA ---------------------------
    esp_zb_attribute_list_t *esp_zb_ota_cluster = create_zb_ota_cluter();

    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();    
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_measure_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_ota_cluster(esp_zb_cluster_list, esp_zb_ota_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
    
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
    //ESP_ERROR_CHECK(zb_register_ota_upgrade_client_device());
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