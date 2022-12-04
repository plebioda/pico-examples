#ifndef BME280_I2C_DEV_H
#define BME280_I2C_DEV_H

#include "bme280.h"
#include "hardware/i2c.h"

#ifndef BME280_hPA
#define BME280_hPA(v) (0.01 * (v))
#endif


#define BME280_DATA_INIT_VALUE {\
    .pressure = -1,\
    .humidity = -1,\
    .temperature = -1\
}

struct bme280_i2c_dev {
    i2c_inst_t *i2c;
    uint8_t addr;
    struct bme280_dev bme280_dev;
    uint32_t measurement_delay_us;
};

int8_t bme280_i2c_dev_init(struct bme280_i2c_dev *dev, i2c_inst_t *i2c, uint8_t addr);
int8_t bme280_i2c_dev_read_data(struct bme280_i2c_dev *dev, struct bme280_data *data);

#endif // BME280_I2C_DEV_H