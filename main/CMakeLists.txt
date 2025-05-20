if (PROJECT_VER) 
    message("Version: Found predefined ${PROJECT_VER}")

    set(OTA_IMAGE_VERSION ${PROJECT_VER})
    string(REPLACE "." "" OTA_IMAGE_FILE_VERSION ${PROJECT_VER})
else()
    set(OTA_IMAGE_VERSION "1.0.0")
    set(OTA_IMAGE_FILE_VERSION 100)
endif()

string(TIMESTAMP OTA_IMAGE_FILE_DATE "%Y%m%d")

message("OTA Version: ${OTA_IMAGE_VERSION}")
message("OTA File Version: ${OTA_IMAGE_FILE_VERSION}")
message("OTA Build date: ${OTA_IMAGE_FILE_DATE}")


# string(CONCAT OTA_IMAGE_FILE_DATE ${CMAKE_CURRENT_YEAR} ${CMAKE_CURRENT_MONTH} ${CMAKE_CURRENT_DAY})
# string(TIMESTAMP OTA_IMAGE_FILE_DATE "%Y%m%d")
# add_definitions(-DOTA_IMAGE_VERSION=\"${OTA_IMAGE_VERSION}\")
# add_definitions(-DOTA_IMAGE_FILE_VERSION=${OTA_IMAGE_FILE_VERSION})
# add_definitions(-DOTA_IMAGE_FILE_DATE=\"${OTA_IMAGE_FILE_DATE}\")

idf_component_register(
    SRCS 
    "zigbee.c"
    INCLUDE_DIRS "." "../common" "../sensor" "../helpers"
)

add_definitions(-DOTA_IMAGE_VERSION=\"${OTA_IMAGE_VERSION}\")
add_definitions(-DOTA_IMAGE_FILE_VERSION=${OTA_IMAGE_FILE_VERSION})
add_definitions(-DOTA_IMAGE_FILE_DATE=\"${OTA_IMAGE_FILE_DATE}\")