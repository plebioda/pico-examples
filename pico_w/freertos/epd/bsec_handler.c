#include "bsec_handler.h"

#include <stdio.h>
#include "util.h"

static const bsec_virtual_sensor_t bsec_virtual_sensor_all[BSEC_NUMBER_OUTPUTS] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_COMPENSATED_GAS,
    BSEC_OUTPUT_GAS_PERCENTAGE,
};

static const char *bsec_output_to_str(bsec_virtual_sensor_t sensor_id)
{
    switch (sensor_id)
    {
    case BSEC_OUTPUT_IAQ:
        return "iaq";
    case BSEC_OUTPUT_STATIC_IAQ:
        return "static_iaq";
    case BSEC_OUTPUT_CO2_EQUIVALENT:
        return "co2_equivalent";
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        return "breath_voc_equivalent";
    case BSEC_OUTPUT_RAW_TEMPERATURE:
        return "raw_temperature";
    case BSEC_OUTPUT_RAW_PRESSURE:
        return "raw_pressure";
    case BSEC_OUTPUT_RAW_HUMIDITY:
        return "raw_humidity";
    case BSEC_OUTPUT_RAW_GAS:
        return "raw_gas";
    case BSEC_OUTPUT_STABILIZATION_STATUS:
        return "stabilization_status";
    case BSEC_OUTPUT_RUN_IN_STATUS:
        return "run_in_status";
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        return "compensated_temperature";
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        return "compensated_humidity";
    case BSEC_OUTPUT_COMPENSATED_GAS:
        return "compensated_gas";
    case BSEC_OUTPUT_GAS_PERCENTAGE:
        return "gas_percentage";
    default:
        return "unknown";
    }
}

static const char *bsec_accuracy_to_str(uint8_t accuracy)
{
    switch (accuracy)
    {
    case 0:
        return "UNRELIABLE";
    case 1:
        return "LOW_ACCURACY";
    case 2:
        return "MEDIUM_ACCURACY";
    case 3:
        return "HIGH_ACCURACY";
    default:
        return "UNKNOWN_ACCURACY";
    }
}

static int8_t bsec_handler_set_bme_settins(struct bsec_handler *bsec_handler, bsec_bme_settings_t *settings)
{
    int8_t ret;

    struct bme68x_conf bme68x_conf;
    ret = bme68x_i2c_dev_get_conf(bsec_handler->dev, &bme68x_conf);
    if (ret)
    {
        printf("bsec_handler_set_bme_settins: bme68x_i2c_dev_get_conf returned %d\n", ret);
        return ret;
    }

    struct bme68x_heatr_conf bme68x_heatr_conf;
    ret = bme68x_i2c_dev_get_heatr_conf(bsec_handler->dev, &bme68x_heatr_conf);
    if (ret)
    {
        printf("bsec_handler_set_bme_settins: bme68x_i2c_dev_get_heatr_conf returned %d\n", ret);
        return ret;
    }
    
    bme68x_conf.os_hum = settings->humidity_oversampling;
    bme68x_conf.os_temp = settings->temperature_oversampling;
    bme68x_conf.os_pres = settings->pressure_oversampling;

    bme68x_heatr_conf.enable = settings->run_gas;
    bme68x_heatr_conf.heatr_temp = settings->heater_temperature;
    bme68x_heatr_conf.heatr_dur = settings->heating_duration;

    printf("bsec_handler_set_bme_settins: os_hum = %d, os_temp = %d, os_pres = %d, heatr.enable = %d, heatr.temp = %d, heatr.dur = %d\n",
        bme68x_conf.os_hum,
        bme68x_conf.os_temp,
        bme68x_conf.os_pres,
        bme68x_heatr_conf.enable,
        bme68x_heatr_conf.heatr_temp,
        bme68x_heatr_conf.heatr_dur
    );

    ret = bme68x_i2c_dev_set_conf(bsec_handler->dev, &bme68x_conf);
    if (ret)
    {
        printf("bsec_handler_set_bme_settins: bme68x_i2c_dev_set_conf returned %d\n", ret);
        return ret;
    }

    ret = bme68x_i2c_dev_set_heatr_conf(bsec_handler->dev, &bme68x_heatr_conf);
    if (ret)
    {
        printf("bsec_handler_set_bme_settins: bme68x_i2c_dev_set_heatr_conf returned %d\n", ret);
        return ret;
    }

    return 0;
}

static int8_t bsec_handler_process_data(struct bsec_handler *bsec_handler, int64_t timestamp, bsec_bme_settings_t *bsec_bme_settings, struct bme68x_data *bme68x_data)
{
    bsec_input_t inputs[BSEC_MAX_PHYSICAL_SENSOR];
    uint8_t num_inputs = 0;

    if (!(bme68x_data->status & BME68X_NEW_DATA_MSK))
    {
        printf("bsec_handler_process_data: no new data, status = 0x%x\n", bme68x_data->status);
        return 0;
    }

    if (bsec_bme_settings->process_data & BSEC_PROCESS_TEMPERATURE)
    {
        inputs[num_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
        inputs[num_inputs].signal = bme68x_data->temperature;
        inputs[num_inputs].signal_dimensions = 1.0f;
        inputs[num_inputs].time_stamp = timestamp;
        num_inputs++;

        /* Temperature offset from the real temperature due to external heat sources */
        inputs[num_inputs].sensor_id = BSEC_INPUT_HEATSOURCE;
        inputs[num_inputs].signal = bsec_handler->heatsource;
        inputs[num_inputs].signal_dimensions = 1.0f;
        inputs[num_inputs].time_stamp = timestamp;
        num_inputs++;
    }

    if (bsec_bme_settings->process_data & BSEC_PROCESS_HUMIDITY)
    {
        inputs[num_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
        inputs[num_inputs].signal = bme68x_data->humidity;
        inputs[num_inputs].signal_dimensions = 1.0f;
        inputs[num_inputs].time_stamp = timestamp;
        num_inputs++;
    }

    if (bsec_bme_settings->process_data & BSEC_PROCESS_PRESSURE)
    {
        inputs[num_inputs].sensor_id = BSEC_INPUT_PRESSURE;
        inputs[num_inputs].signal = bme68x_data->pressure;
        inputs[num_inputs].signal_dimensions = 1.0f;
        inputs[num_inputs].time_stamp = timestamp;
        num_inputs++;
    }
    if (bsec_bme_settings->process_data & BSEC_PROCESS_GAS)
    {
        inputs[num_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
        inputs[num_inputs].signal = bme68x_data->gas_resistance;
        inputs[num_inputs].signal_dimensions = 1.0f;
        inputs[num_inputs].time_stamp = timestamp;
        num_inputs++;
    }

    if (!num_inputs)
    {
        printf("bsec_handler_process_data: no inputs for processing, process_data = 0x%x\n", bsec_bme_settings->process_data);
        return 0;
    }


    bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
    uint8_t num_outputs = BSEC_NUMBER_OUTPUTS;
    
    bsec_library_return_t ret;

    ret = bsec_do_steps(inputs, num_inputs, outputs, &num_outputs);
    if (ret != BSEC_OK)
    {
        printf("bsec_handler_process_data: bsec_do_steps returned %d\n", ret);
        return ret;
    }

    if (!num_outputs)
    {
        printf("bsec_handler_process_data: bsec_do_steps returned 0 outputs\n");
        return -1;
    }

    printf("-------------------------------------------------------------------\n");
    for (uint8_t i = 0; i < num_outputs; i++)
    {
        const char *output_name = bsec_output_to_str(outputs[i].sensor_id);

        printf("%24s = %10.2f [%s]\n", output_name, outputs[i].signal, bsec_accuracy_to_str(outputs[i].accuracy));
    }
    printf("-------------------------------------------------------------------\n");

    return 0;
}

int8_t bsec_handler_init(struct bsec_handler *bsec_handler, struct bme68x_i2c_dev *bme68x_i2c_dev)
{
    bsec_library_return_t ret;

    ret = bsec_init();
    if (ret != BSEC_OK)
    {
        printf("bsec_handler_init: bsec_init returned %d\n", ret);
        return ret;
    }

    bsec_version_t bsec_version;
    ret = bsec_get_version(&bsec_version);
    if (ret)
    {
        printf("bsec_handler_init: bsec_init returned %d\n", ret);
        return ret;
    }

    printf("bsec_handler_init: version %d.%d.%d.%d\n",
            bsec_version.major,
            bsec_version.minor,
            bsec_version.major_bugfix,
            bsec_version.minor_bugfix
    );


    for (int i = 0; i < BSEC_NUMBER_OUTPUTS; i++)
    {
        bsec_handler->virtualSensors[i].sensor_id = bsec_virtual_sensor_all[i];
        bsec_handler->virtualSensors[i].sample_rate = BSEC_SAMPLE_RATE_DISABLED;
    }

    uint8_t n_required_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;
    ret = bsec_update_subscription(bsec_handler->virtualSensors, BSEC_NUMBER_OUTPUTS, bsec_handler->sensorSettings, &n_required_sensor_settings);
    if (ret)
    {
        printf("bsec_handler_init: bsec_update_subscription returned %d\n", ret);
        return ret;
    }

    printf("bsec_handler_init: n_required_sensor_settings = %d\n", n_required_sensor_settings);

    bsec_handler->dev = bme68x_i2c_dev;
    bsec_handler->heatsource = 0.0f;

    return 0;
}

int8_t bsec_handler_subscribe_all(struct bsec_handler *bsec_handler, float sample_rate)
{
    for (int i = 0; i < BSEC_NUMBER_OUTPUTS; i++)
    {
        bsec_handler->virtualSensors[i].sensor_id = bsec_virtual_sensor_all[i];
        bsec_handler->virtualSensors[i].sample_rate = sample_rate;
    }

    uint8_t n_required_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;
    int8_t ret = bsec_update_subscription(bsec_handler->virtualSensors, BSEC_NUMBER_OUTPUTS, bsec_handler->sensorSettings, &n_required_sensor_settings);
    if (ret)
    {
        printf("bsec_handler_subscribe_all: bsec_update_subscription returned %d\n", ret);
        return ret;
    }

    printf("bsec_handler_subscribe_all: n_required_sensor_settings = %d\n", n_required_sensor_settings);

    return ret;
}

int8_t bsec_handler_run(struct bsec_handler *bsec_handler, int64_t timestamp, int64_t *next_call_timestamp)
{
    bsec_library_return_t ret;

    bsec_bme_settings_t bsec_bme_settings;
    ret = bsec_sensor_control(timestamp, &bsec_bme_settings);
    if (ret)
    {
        printf("bsec_handler_run: bsec_sensor_control returned %d\n", ret);
        return ret;
    }

    if (!bsec_bme_settings.trigger_measurement)
    {
        printf("bsec_handler_run: no measurement required\n");
        return 0;
    }

    *next_call_timestamp = bsec_bme_settings.next_call;

    ret = bsec_handler_set_bme_settins(bsec_handler, &bsec_bme_settings);
    if (ret)
    {
        printf("bsec_handler_run: bsec_handler_set_bme_settins returned %d\n", ret);
        return ret;
    }

    struct bme68x_data bme68x_data = BME68X_DATA_INIT_VALUE;
    ret = bme68x_i2c_dev_read_data(bsec_handler->dev, &bme68x_data);
    if (ret)
    {
        printf("bsec_handler_run: bme68x_i2c_dev_read_data returned %d\n", ret);
        return ret;
    }

    printf("bme68x_data: T: %.2lf C H: %.2lf %% P: %.2lf hPA G: %.2lf Ohm\n",
        bme68x_data.temperature,
        bme68x_data.humidity,
        BME68X_hPA(bme68x_data.pressure),
        bme68x_data.gas_resistance
    );

    ret = bsec_handler_process_data(bsec_handler, timestamp, &bsec_bme_settings, &bme68x_data);
    if (ret)
    {
        printf("bsec_handler_run: bsec_handler_process_data returned %d\n", ret);
        return ret;
    }

    return 0;
}

int8_t bsec_handler_get_state(struct bsec_handler *bsec_handler, struct bsec_handler_state *state)
{
    int8_t ret;
    uint8_t work_buffer_state[BSEC_MAX_STATE_BLOB_SIZE];
    
    ret = bsec_get_state(0, state->serialized_state, sizeof(state->serialized_state), work_buffer_state, sizeof(work_buffer_state), &state->serialized_state_size);
    if (ret)
    {
        printf("bsec_handler_run: bsec_get_state returned %d\n", ret);
        return ret;
    }

    return 0;
}

int8_t bsec_handler_set_state(struct bsec_handler *bsec_handler, const struct bsec_handler_state *state)
{
    int8_t ret;
    uint8_t work_buffer_state[BSEC_MAX_STATE_BLOB_SIZE];

    ret = bsec_set_state(state->serialized_state, state->serialized_state_size, work_buffer_state, sizeof(work_buffer_state));
    if (ret)
    {
        printf("bsec_handler_set_state: bsec_set_state returned %d\n", ret);
        return ret;
    }

    return 0;
}