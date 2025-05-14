import {deviceAddCustomCluster} from 'zigbee-herdsman-converters/lib/modernExtend';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';
import * as constants from "zigbee-herdsman-converters/lib/constants";
import {logger} from 'zigbee-herdsman-converters/lib/logger';
import * as utils from 'zigbee-herdsman-converters/lib/utils';
import {Zcl} from "zigbee-herdsman";

const e = exposes.presets;
const ea = exposes.access;

const manufacturerOptions = {manufacturerCode: 0x9252};

const customCluster = {
    name:"waterLevelMeasurement",
    id: 0xff00
};

const waterLevelAttributes = {
    currentLevel: 0x0001,
    maxLevel: 0x0002,
    minLevel: 0x0003,
    totalVolume: 0x0004,
    storageVolume: 0x0005,
    retentionVolume: 0x0006,
    currentVolume: 0x0007,
    currentStorageVolume: 0x0008,
    currentRetentionVolume: 0x0009
};

const fzLocal = {
    WaterLevelConfig: {
        cluster: customCluster.name,
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const payload = {};
            if (msg.data.currentLevel !== undefined) {
                payload.currentLevel = Number.parseFloat(msg.data.currentLevel);
            }

            if (msg.data.minLevel !== undefined) {
                payload.minLevel = Number.parseFloat(msg.data.minLevel);
            }
            
            if (msg.data.maxLevel !== undefined) {
                payload.maxLevel = Number.parseFloat(msg.data.maxLevel);
            }
            
            if (msg.data.totalVolume !== undefined) {
                payload.totalVolume = Number.parseFloat(msg.data.totalVolume);
            }

            if (msg.data.storageVolume !== undefined) {
                payload.storageVolume = Number.parseFloat(msg.data.storageVolume);
            }

            if (msg.data.retentionVolume !== undefined) {
                payload.retentionVolume = Number.parseFloat(msg.data.retentionVolume);
            }
            
            if (msg.data.currentVolume !== undefined) {
                payload.currentVolume = Number.parseFloat(msg.data.currentVolume);
            }

            if (msg.data.currentStorageVolume !== undefined) {
                payload.currentStorageVolume = Number.parseFloat(msg.data.currentStorageVolume);
            }

            if (msg.data.currentRetentionVolume !== undefined) {
                payload.currentRetentionVolume = Number.parseFloat(msg.data.currentRetentionVolume);
            }
            return payload;
        }
    },
};

const tzLocal = {
    WaterLevelConfig: {
        key: ['currentLevel', 
            'minLevel', 
            'maxLevel', 
            'totalVolume', 
            'storageVolume', 
            'retentionVolume',
            'currentVolume', 
            'currentStorageVolume', 
            'currentRetentionVolume'],
        
        convertSet: async (entity, key, value, meta) => {
            const state = {};
            logger.debug("key " + key);
            let payload = {[utils.printNumberAsHex(waterLevelAttributes[key], 4)]: {'value': value, 'type': Zcl.DataType.UINT16}};
            await entity.write(customCluster.name, payload, manufacturerOptions.manufacturerCode);
            return {state: {[key]: value}};
        },
        convertGet: async (entity, key, meta) => {
            logger.debug("Read Key: " + key);
            await entity.read(customCluster.name, [key], manufacturerOptions.manufacturerCode);
        },
    },
};

export default {
    zigbeeModel: ['LLSensor.001'],
    model: 'LLSensor.001',
    vendor: 'TheLittleFlea',
    description: 'DIY The Little Flea custom Wwater sensor',
    ota: true,
    extend: [
        deviceAddCustomCluster(customCluster.name, {
            ID: customCluster.id,
            manufacturerCode: manufacturerOptions.manufacturerCode,
            attributes: {
                currentLevel: {
                    ID: waterLevelAttributes.currentLevel, 
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                minLevel: {
                    ID: waterLevelAttributes.minLevel,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                maxLevel: {
                    ID: waterLevelAttributes.maxLevel,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                totalVolume: {
                    ID: waterLevelAttributes.totalVolume,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                storageVolume: {
                    ID: waterLevelAttributes.storageVolume,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                retentionVolume: {
                    ID: waterLevelAttributes.retentionVolume,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                currentVolume: {
                    ID: waterLevelAttributes.currentVolume,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                currentStorageVolume: {
                    ID: waterLevelAttributes.currentStorageVolume,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
                currentRetentionVolume: {
                    ID: waterLevelAttributes.currentRetentionVolume,
                    type: Zcl.DataType.UINT16,
                    manufacturerCode: manufacturerOptions.manufacturerCode,
                },
            },
            commands: {},
            commandsResponse: {},
        }),
    ],
    fromZigbee: [
        fzLocal.WaterLevelConfig,
    ],
    toZigbee: [
        tzLocal.WaterLevelConfig,
    ],
    exposes: [
        e.numeric("currentLevel", ea.STATE_GET)
            .withUnit("cm")
            .withDescription("Current level of water.")
            .withValueMin(0)
            .withValueMax(450)
            .withValueStep(1), 
        
        e.numeric("minLevel", ea.ALL)
            .withUnit("cm")
            .withDescription("Min level of the tank.")
            .withValueMin(0)
            .withValueMax(450)
            .withValueStep(1),  
        
        e.numeric("maxLevel", ea.ALL)
            .withUnit("cm")
            .withDescription("Max level of the tank.")
            .withValueMin(0)
            .withValueMax(450)
            .withValueStep(1),
        
        e.numeric("currentVolume", ea.STATE_GET)
            .withUnit("L")
            .withDescription("Current volume of liquid in the tank."), 
        
        e.numeric("currentStorageVolume", ea.STATE_GET)
            .withUnit("L")
            .withDescription("Current storage volume of liquid in the tank."), 
        
        e.numeric("currentRetentionVolume", ea.STATE_GET)
            .withUnit("L")
            .withDescription("Current retention volume of liquid in the tank."),  
        
        e.numeric("totalVolume", ea.ALL)
            .withUnit("L")
            .withDescription("Total volume of the tank.")
            .withValueMin(0)
            .withValueMax(15000)
            .withValueStep(1), 
        
        e.numeric("storageVolume", ea.ALL)
            .withUnit("L")
            .withDescription("Storage volume part of the tank.")
            .withValueMin(0)
            .withValueMax(15000)
            .withValueStep(1), 
        
        e.numeric("retentionVolume", ea.ALL)
            .withUnit("L")
            .withDescription("Retention volume part of the tank.")
            .withValueMin(0)
            .withValueMax(15000)
            .withValueStep(1), 
    ],
    configure: async (device, coordinatorEndpoint, logger) => {
        const options = {manufacturerCode: manufacturerOptions.manufacturerCode};

        device.endpoints.forEach(async (ep) => {
            if (typeof ep !== "undefined") {
                await reporting.bind(ep, coordinatorEndpoint, [customCluster.name]);

                // await ep.configureReporting(
                //     customCluster.name,
                //     [
                //         {
                //             attribute: "currentLevel",
                //             minimumReportInterval: 0,
                //             maximumReportInterval: constants.repInterval.MINUTE,
                //             reportableChange: 1,
                //         },
                //         {
                //             attribute: "currentVolume",
                //             minimumReportInterval: 0,
                //             maximumReportInterval: constants.repInterval.MINUTE,
                //             reportableChange: 1,
                //         },
                //         {
                //             attribute: "currentStorageVolume",
                //             minimumReportInterval: 0,
                //             maximumReportInterval: constants.repInterval.MINUTE,
                //             reportableChange: 1,
                //         },
                //         {
                //             attribute: "currentRetentionVolume",
                //             minimumReportInterval: 0,
                //             maximumReportInterval: constants.repInterval.MINUTE,
                //             reportableChange: 1,
                //         },
                //     ],
                //     options,
                // );

                await ep.read(customCluster.name, 
                    [
                        waterLevelAttributes.currentLevel,
                        waterLevelAttributes.maxLevel,
                        waterLevelAttributes.minLevel,
                        waterLevelAttributes.retentionVolume,
                        waterLevelAttributes.storageVolume,
                        waterLevelAttributes.totalVolume,
                        waterLevelAttributes.currentVolume,
                        waterLevelAttributes.currentStorageVolume,
                        waterLevelAttributes.currentRetentionVolume
                    ], manufacturerOptions.manufacturerCode);
            }
        });
    },
};