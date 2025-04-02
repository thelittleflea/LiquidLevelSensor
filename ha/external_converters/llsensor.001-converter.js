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
    currentLevel: 0x0001
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
    }
};

const tzLocal = {
    currentLevel: {
        key: ['currentLevel'],
        convertGet: async (entity, key, meta) => {
            await entity.read(customCluster.name, ['currentLevel'], manufacturerOptions);
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
                    type: dataType.UINT16
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
    ],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(10);
        await reporting.bind(endpoint, coordinatorEndpoint, [customCluster.name]);
    },
};