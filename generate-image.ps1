$manuId="0x9252"
$imageType="0x1011"
$tagId="0x0000"
$version = [System.IO.File]::ReadAllText("version.txt")
$fileVersion = [int]$version.Replace(".", "")
$hwMinVersion="0x0001"
$hwMaxVersion="0x0003"
$fileName="LiquidLevelSensor_$version.bin"
$tagFile="build/LiquidLevelSensor.bin"
$create="build/$fileName"
$destination="\\10.0.0.200\repo\iot\ota"


idf.py set-target esp32h2

idf.py fullclean

idf.py set-target esp32h2

idf.py build

python J:\Iot\esp-idf\frameworks\esp-zigbee-sdk\tools\image_builder_tool\image_builder_tool.py --create $create --manuf-id $manuId --image-type $imageType --version $fileVersion --tag-id $tagId --min-hw-ver $hwMinVersion --max-hw-ver $hwMaxVersion --tag-file $tagFile 

# 74:4d:bd:ff:fe:61:e7:ce
# python J:\Iot\esp-idf\frameworks\esp-zigbee-sdk\tools\image_builder_tool\image_builder_tool.py --create build/on_off_light_bulb.bin --manuf-id 0x1001 --image-type 0x1011 --version 0x01010102 --upgrade-dest 01:23:45:67:89:AB:CD:EF --tag-id 0x0000 --tag-file build/on_off_light_bulb.bin

Copy-Item $create -Destination "$destination\liquidlevelsensor\"

$fileInfo = Get-Item "$destination\liquidlevelsensor\$fileName"
$size = $fileInfo.Length
# $hash = Get-FileHash "$destination\liquidlevelsensor\$fileName" -Algorithm SHA512
$hash = certUtil -hashfile "$destination\liquidlevelsensor\$fileName"  SHA512 | Select -Index 1;
#$hashBytes = [System.Text.Encoding]::Unicode.GetBytes($hash.Hash)

$json = Get-Content -Path "$destination\ota_index.json" -Raw | ConvertFrom-Json
if (-Not $json)
{
    $json = @()
}

$entry = $json | Where-Object { $_.modelId -eq "LLSensor.001" }

if (-Not $entry) {
    $newEntry = [pscustomobject]@{
        fileName = "LiquidLevelSensor_$version.bin"
        fileSize = $size
        url = "/share/ota/iot/ota/liquidlevelsensor/$fileName"
        path = "/share/ota/iot/ota/liquidlevelsensor/$fileName"
        modelId = "LLSensor.001"
        fileVersion = $fileVersion
        imageType = [uint32]$imageType
        manufacturerCode = [uint32]$manuId
        sha512 = $hash #[System.Convert]::ToBase64String($hashBytes)
    }
    $json += $newEntry
} else {
    $entry.fileName = "LiquidLevelSensor_$version.bin"
    $entry.fileSize = $size
    $entry.url = "/share/ota/iot/ota/liquidlevelsensor/$fileName"
    $entry.path = "/share/ota/iot/ota/liquidlevelsensor/$fileName"
    $entry.modelId = "LLSensor.001"
    $entry.fileVersion = $fileVersion
    $entry.imageType = [uint32]$imageType
    $entry.manufacturerCode = [uint32]$manuId
    $entry.sha512 = $hash #[System.Convert]::ToBase64String($hashBytes)
}
if ($json -is [Array] -And $json.Count -gt 1)
{
    $json | ConvertTo-Json | Set-Content -Path "$destination\ota_index.json"
} else {
    ConvertTo-Json -InputObject @($json) | Set-Content -Path "$destination\ota_index.json"
}