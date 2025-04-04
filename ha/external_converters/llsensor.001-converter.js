import {deviceAddCustomCluster} from 'zigbee-herdsman-converters/lib/modernExtend';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';

const e = exposes.presets;
const ea = exposes.access;

const manufacturerOptions = {manufacturerCode: 0x2424};

const customCluster = {
    name:"waterLevelMeasurement",
    id: 0xff00
};

const waterLevelAttributes = {
    currentLevel: 0x0001,
    minLevel: 0x0002,
    maxLevel: 0x0003,
    totalVolume: 0x0004,
    storageVolume: 0x0005,
    retentionVolume: 0x0006
};

const dataType = {
    UINT32: 35,
    UINT16: 33,
    UINT8: 0x20,
    ENUM8: 0x30,
    BITMAP16: 25,
}

const fzLocal = {
    currentLevel: {
        cluster: customCluster.name,
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            let currentLevel = Number.parseFloat(msg.data.currentLevel);
            return {currentLevel};
        }
    },
    minLevel: {
        cluster: customCluster.name,
        type: ['readResponse'],
        convert: (model, msg, publish, options, meta) => {
            let minLevel = Number.parseFloat(msg.data.minLevel);
            return {minLevel};
        }
    },
    maxLevel: {
        cluster: customCluster.name,
        type: ['readResponse'],
        convert: (model, msg, publish, options, meta) => {
            let maxLevel = Number.parseFloat(msg.data.maxLevel);
            return {maxLevel};
        }
    },
    totalVolume: {
        cluster: customCluster.name,
        type: ['readResponse'],
        convert: (model, msg, publish, options, meta) => {
            let totalVolume = Number.parseFloat(msg.data.totalVolume);
            return {totalVolume};
        }
    },
    storageVolume: {
        cluster: customCluster.name,
        type: ['readResponse'],
        convert: (model, msg, publish, options, meta) => {
            let storageVolume = Number.parseFloat(msg.data.storageVolume);
            return {storageVolume};
        }
    },
    retentionVolume: {
        cluster: customCluster.name,
        type: ['readResponse'],
        convert: (model, msg, publish, options, meta) => {
            let retentionVolume = Number.parseFloat(msg.data.retentionVolume);
            return {retentionVolume};
        }
    }
};

const tzLocal = {
    currentLevel: {
        key: ['currentLevel'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['currentLevel'], manufacturerOptions);
        },
    },
    minLevel: {
        key: ['minLevel'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['minLevel'], manufacturerOptions);
        },
    },
    maxLevel: {
        key: ['minLevel'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['minLevel'], manufacturerOptions);
        },
    },
    totalVolume: {
        key: ['totalVolume'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['totalVolume'], manufacturerOptions);
        },
    },
    storageVolume: {
        key: ['storageVolume'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['storageVolume'], manufacturerOptions);
        },
    },
    retentionVolume: {
        key: ['retentionVolume'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['retentionVolume'], manufacturerOptions);
        },
    }
};

export default {
    zigbeeModel: ['LLSensor.001'],
    model: 'LLSensor.001',
    vendor: 'TheLittleFlea',
    description: 'DIY The Little Flea custom Wwater sensor',
    extend: [
        deviceAddCustomCluster(customCluster.name, {
            ID: customCluster.id,
            manufacturerCode: manufacturerOptions.manufacturerCode,
            attributes: {
                currentLevel: {
                    ID: waterLevelAttributes.currentLevel,
                    type: dataType.UINT16,
                    manufacturerCode: sunricherManufacturerCode,
                },
                minLevel: {
                    ID: waterLevelAttributes.minLevel,
                    type: dataType.UINT16,
                    manufacturerCode: sunricherManufacturerCode,
                },
                maxLevel: {
                    ID: waterLevelAttributes.maxLevel,
                    type: dataType.UINT16,
                    manufacturerCode: sunricherManufacturerCode,
                },
                totalVolume: {
                    ID: waterLevelAttributes.totalVolume,
                    type: dataType.UINT16,
                    manufacturerCode: sunricherManufacturerCode,
                },
                storageVolume: {
                    ID: waterLevelAttributes.storageVolume,
                    type: dataType.UINT16,
                    manufacturerCode: sunricherManufacturerCode,
                },
                retentionVolume: {
                    ID: waterLevelAttributes.retentionVolume,
                    type: dataType.UINT16,
                    manufacturerCode: sunricherManufacturerCode,
                },
            },
            commands: {},
            commandsResponse: {},
        }),
    ],
    fromZigbee: [
        fzLocal.currentLevel
    ],
    toZigbee: [
        tzLocal.currentLevel
    ],
    exposes: [
        e.numeric("currentLevel", ea.STATE)
            .withUnit("cm")
            .withDescription("Current level of water.")
            .withValueMin(0)
            .withValueMax(450)
            .withValueStep(1), 
        
            e.numeric("minLevel", ea.STATE)
            .withUnit("cm")
            .withDescription("Min level of the tank.")
            .withValueMin(0)
            .withValueMax(450)
            .withValueStep(1),  
        
            e.numeric("maxLevel", ea.STATE)
            .withUnit("cm")
            .withDescription("Max level of the tank.")
            .withValueMin(0)
            .withValueMax(450)
            .withValueStep(1),  
        
            e.numeric("totalVolume", ea.STATE)
            .withUnit("L")
            .withDescription("Total volume of the tank.")
            .withValueMin(0)
            .withValueMax(15000)
            .withValueStep(1), 
        
            e.numeric("storageVolume", ea.STATE)
            .withUnit("L")
            .withDescription("Storage volume part of the tank.")
            .withValueMin(0)
            .withValueMax(15000)
            .withValueStep(1), 
        
            e.numeric("retentionVolume", ea.STATE)
            .withUnit("L")
            .withDescription("Retention volume part of the tank.")
            .withValueMin(0)
            .withValueMax(15000)
            .withValueStep(1), 
    ],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(10);
        await reporting.bind(endpoint, coordinatorEndpoint, [customCluster.name]);
    },
};