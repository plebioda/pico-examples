#ifndef BME68x_I2C_DEV_H
#define BME68x_I2C_DEV_H

#include "bme68x.h"
#include "hardware/i2c.h"

#ifndef BME68X_hPA
#define BME68X_hPA(v) (0.01 * (v))
#endif

#define BME68X_DATA_INIT_VALUE {\
    .pressure = -1,\
    .humidity = -1,\
    .temperature = -1\
}

struct bme68x_i2c_dev {
    i2c_inst_t *i2c;
    uint8_t addr;
    struct bme68x_dev bme68x_dev;
    struct bme68x_conf bme68x_conf;
    struct bme68x_heatr_conf bme68x_heatr_conf;
    uint32_t measurement_delay_us;
};

int8_t bme68x_i2c_dev_init(struct bme68x_i2c_dev *dev, i2c_inst_t *i2c, uint8_t addr);
int8_t bme68x_i2c_dev_read_data(struct bme68x_i2c_dev *dev, struct bme68x_data *data);
int8_t bme68x_i2c_dev_get_conf(struct bme68x_i2c_dev *dev, struct bme68x_conf *bme68x_conf);
int8_t bme68x_i2c_dev_set_conf(struct bme68x_i2c_dev *dev, struct bme68x_conf *bme68x_conf);
int8_t bme68x_i2c_dev_set_heatr_conf(struct bme68x_i2c_dev *dev, struct bme68x_heatr_conf *bme68x_heatr_conf);
int8_t bme68x_i2c_dev_get_heatr_conf(struct bme68x_i2c_dev *dev, struct bme68x_heatr_conf *bme68x_heatr_conf);

#endif // BME68x_I2C_DEV_H