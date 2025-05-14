#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE                   false                                   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                            ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                               3000                                    /* 3000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT                      10                                      /* esp temperature sensor device endpoint, used for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK                 ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */
#define ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT           0xff00

#define ESP_LEVEL_SENSOR_UPDATE_INTERVAL            (60)                                   /* Local sensor update interval (second) */
#define ESP_LEVEL_SENSOR_MIN_VALUE                  (0)                                    /* Local sensor min measured value (in meter) */
#define ESP_LEVEL_SENSOR_MAX_VALUE                  (4)                                    /* Local sensor max measured value (in meter) */

#define LIQUID_LEVEL_SERVER_ENDPOINT                0x01
#define LIQUID_LEVEL_CLUSTER_ID                     0xff00
#define LIQUID_LEVEL_COMMAND_RESP                   0x0001

#define ATTR_CURRENT_LEVEL_ATTRIBUTE_ID             0x0001
#define ATTR_CURRENT_LEVEL_DEFAULT_VALUE            ((uint16_t)0)

#define ATTR_MAX_TANK_LEVEL_ID                      0x0002
#define ATTR_MAX_TANK_LEVEL                         ((uint16_t)100)

#define ATTR_MIN_TANK_LEVEL_ID                      0x0003
#define ATTR_MIN_TANK_LEVEL                         ((uint16_t)0)

#define ATTR_TANK_VOLUME_ID                         0x0004
#define ATTR_TANK_VOLUME                            ((uint16_t)2000)

#define ATTR_TANK_STORAGE_VOLUME_ID                 0x0005
#define ATTR_TANK_STORAGE_VOLUME                    ((uint16_t)1000)

#define ATTR_TANK_RETENTION_VOLUME_ID               0x0006
#define ATTR_TANK_RETENTION_VOLUME                  ((uint16_t)1000)

#define ATTR_TANK_CURRENT_VOLUME_ID                 0x0007
#define ATTR_TANK_CURRENT_VOLUME                    ((uint16_t)0)

#define ATTR_TANK_CURRENT_STORAGE_VOLUME_ID         0x0008
#define ATTR_TANK_CURRENT_STORAGE_VOLUME            ((uint16_t)0)

#define ATTR_TANK_CURRENT_RETENTION_VOLUME_ID       0x0009
#define ATTR_TANK_CURRENT_RETENTION_VOLUME          ((uint16_t)0)

#define APPLICATION_VERSION                         ((uint16_t)0x01)
#define STACK_VERSION                               ((uint16_t)0x02)
#define HW_VERSION                                  ((uint16_t)0x02)
#define SW_BUILD_ID                                 PROJECT_VER
#define POWER_SOURCE                                ((uint16_t)0x01)
#define DATE_CODE                                   __DATE__

/* Basic manufacturer information */        
#define ESP_MANUFACTURER_NAME                       "\x0d""TheLittleFlea"                   /* Customized manufacturer name */
#define ESP_MANUFACTURER_CODE                       0x9252
#define ESP_MODEL_IDENTIFIER                        "\x0c""LLSensor.001"                    /* Customized model identifier */

/* OTA inofrmation*/
#define OTA_UPGRADE_IMAGE_TYPE                      0x1011                                  /* The attribute indicates the value for the manufacturer of the device */
#define OTA_UPGRADE_RUNNING_FILE_VERSION            0x92520001 /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_DOWNLOADED_FILE_VERSION         ESP_ZB_ZCL_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_DEF_VALUE                             /* The attribute indicates the file version of the downloaded firmware image on the device */
#define OTA_UPGRADE_MAX_DATA_SIZE                   223
#define OTA_ELEMENT_HEADER_LEN                      6       /* OTA element format header size include tag identifier and length field */


#define ESP_ZB_ZC_CONFIG()                                          \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

    #define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                               \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                         \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                                \
    {                                                               \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,       \
    }


/**
 * @name Enumeration for the tag identifier denotes the type and format of the data within the element
 * @anchor esp_ota_element_tag_id_t
 */
typedef enum esp_ota_element_tag_id_e {
    UPGRADE_IMAGE                               = 0x0000,           /*!< Upgrade image */
} esp_ota_element_tag_id_t;