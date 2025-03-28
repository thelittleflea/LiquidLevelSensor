/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false                                   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000                                    /* 3000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT          10                                      /* esp temperature sensor device endpoint, used for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */

#define ESP_LEVEL_SENSOR_UPDATE_INTERVAL (60)                                   /* Local sensor update interval (second) */
#define ESP_LEVEL_SENSOR_MIN_VALUE       (0)                                    /* Local sensor min measured value (in meter) */
#define ESP_LEVEL_SENSOR_MAX_VALUE       (4)                                    /* Local sensor max measured value (in meter) */

#define LIQUID_LEVEL_SERVER_ENDPOINT    0x01
#define LIQUID_LEVEL_CLUSTER_ID         0xff00
#define LIQUID_LEVEL_COMMAND_RESP       0x0001
#define LIQUID_LEVEL_REPORT_ATTRIBUT_ID 0x0000