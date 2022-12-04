#ifndef BSEC_HANDLER_H
#define BSEC_HANDLER_H

#include "bme68x_i2c_dev.h"

#include "bsec_datatypes.h"
#include "bsec_interface.h"

struct bsec_handler
{
    struct bme68x_i2c_dev *dev;
    float heatsource;

    bsec_sensor_configuration_t virtualSensors[BSEC_NUMBER_OUTPUTS];
    bsec_sensor_configuration_t sensorSettings[BSEC_MAX_PHYSICAL_SENSOR];
};

struct bsec_handler_state
{
    uint32_t serialized_state_size;
    uint8_t serialized_state[BSEC_MAX_STATE_BLOB_SIZE];

};


int8_t bsec_handler_init(struct bsec_handler *bsec_handler, struct bme68x_i2c_dev *bme68x_i2c_dev);
int8_t bsec_handler_subscribe_all(struct bsec_handler *bsec_handler, float sample_rate);
int8_t bsec_handler_run(struct bsec_handler *bsec_handler, int64_t timestamp, int64_t *next_call_timestamp);
int8_t bsec_handler_get_state(struct bsec_handler *bsec_handler, struct bsec_handler_state *state);
int8_t bsec_handler_set_state(struct bsec_handler *bsec_handler, const struct bsec_handler_state *state);

#endif // BSEC_HANDLER_H