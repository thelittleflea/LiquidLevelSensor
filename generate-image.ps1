$manuId="0x9252"
$imageType="0x1011"
$version="0x92520003"
$tagId="0x0000"
$hwMinVersion="0x0001"
$hwMaxVersion="0x0003"
$tagFile="build/LiquidLevelSensor.bin"
$create="build/LiquidLevelSensor-$version.bin"
$destination="\\10.0.0.200\repo\iot\ota"


idf.py set-target esp32h2

idf.py fullclean

idf.py set-target esp32h2

idf.py build

python J:\Iot\esp-idf\frameworks\esp-zigbee-sdk\tools\image_builder_tool\image_builder_tool.py --create $create --manuf-id $manuId --image-type $imageType --version $version --tag-id $tagId --min-hw-ver $hwMinVersion --max-hw-ver $hwMaxVersion --tag-file $tagFile 

# 74:4d:bd:ff:fe:61:e7:ce
# python J:\Iot\esp-idf\frameworks\esp-zigbee-sdk\tools\image_builder_tool\image_builder_tool.py --create build/on_off_light_bulb.bin --manuf-id 0x1001 --image-type 0x1011 --version 0x01010102 --upgrade-dest 01:23:45:67:89:AB:CD:EF --tag-id 0x0000 --tag-file build/on_off_light_bulb.bin

Copy-Item $create -Destination "$destination\liquidlevelsensor\"
# zigpy ota generate-index --ota-url-root="./liquidlevelsensor/" "$destination/**/*.bin" >> "$destination\ota_index.json"