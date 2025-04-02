#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE           false                                   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                    ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                       3000                                    /* 3000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT              10                                      /* esp temperature sensor device endpoint, used for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK         ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */
#define ESP_ZB_ZCL_CLUSTER_ID_MEASUREMENT   0xff00

#define ESP_LEVEL_SENSOR_UPDATE_INTERVAL    (60)                                   /* Local sensor update interval (second) */
#define ESP_LEVEL_SENSOR_MIN_VALUE          (0)                                    /* Local sensor min measured value (in meter) */
#define ESP_LEVEL_SENSOR_MAX_VALUE          (4)                                    /* Local sensor max measured value (in meter) */

#define LIQUID_LEVEL_SERVER_ENDPOINT        0x01
#define LIQUID_LEVEL_CLUSTER_ID             0xff00
#define LIQUID_LEVEL_COMMAND_RESP           0x0001
#define LIQUID_LEVEL_REPORT_ATTRIBUT_ID     0x0000

#define CURRENT_LEVEL_ATTRIBUTE_ID          0x0001
#define CURRENT_LEVEL_DEFAULT_VALUE         ((float)0.1)

#define APPLICATION_VERSION                 ((uint16_t)0x01)
#define STACK_VERSION                       ((uint16_t)0x02)
#define HW_VERSION                          ((uint16_t)0x02)
#define SW_BUILD_ID                         "\x4""1.00"
#define POWER_SOURCE                        ((uint16_t)0x01)
#define DATE_CODE                           "\x8""20250401"

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME               "\x0d""TheLittleFlea"                 /* Customized manufacturer name */
#define ESP_MANUFACTURER_CODE               0x2424
#define ESP_MODEL_IDENTIFIER                "\x0c""LLSensor.001"             /* Customized model identifier */

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
