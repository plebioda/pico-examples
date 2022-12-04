#include "bme68x_i2c_dev.h"

#include <stdio.h>
#include <string.h>
// #define BME68X_I2C_LOG_TRAFFIC

static BME68X_INTF_RET_TYPE bme68x_i2c_dev_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    struct bme68x_i2c_dev *dev = (struct bme68x_i2c_dev *)intf_ptr;
    int ret;

    ret = i2c_write_blocking(dev->i2c, dev->addr, &reg_addr, 1, true);
    if (ret != 1) {
        printf("bme68x_i2c_dev_read: i2c_write_blocking(req=0x%x) returned %d\n", reg_addr, ret);
        return ret;
    }

    ret = i2c_read_blocking(dev->i2c, dev->addr, reg_data, len, false);
    if (ret != len) {
        printf("bme68x_i2c_dev_read: i2c_read_blocking(req=0x%x, len=%d) returned %d\n", reg_addr, len, ret);
        return ret;
    }

#ifdef BME68X_I2C_LOG_TRAFFIC
    printf("bme68x_i2c_dev_read(0x%0lx, len=%d) = ", reg_addr, len);
    for (size_t i = 0; i < len; i++)
    {
        printf("0x%lx ", reg_data[i]);
    }
    printf("\n");
#endif

    return BME68X_INTF_RET_SUCCESS;
}
#define USE_ONE_WRITE
static BME68X_INTF_RET_TYPE bme68x_i2c_dev_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    struct bme68x_i2c_dev *dev = (struct bme68x_i2c_dev *)intf_ptr;
    int ret;

#ifdef USE_ONE_WRITE
    uint8_t buff[256];
    buff[0] = reg_addr;
    memcpy(&buff[1], reg_data, len);
    ret = i2c_write_blocking(dev->i2c, dev->addr, buff, 1 + len, false);
    if (ret != len + 1) {
        printf("bme68x_i2c_dev_write: i2c_write_blocking(req=0x%x) returned %d\n", reg_addr, ret);
        return ret;
    }
#else
    ret = i2c_write_blocking(dev->i2c, dev->addr, &reg_addr, 1, true);
    if (ret != 1) {
        printf("bme68x_i2c_dev_write: i2c_write_blocking(req=0x%x) returned %d\n", reg_addr, ret);
        return ret;
    }

    ret = i2c_write_blocking(dev->i2c, dev->addr, reg_data, len, false);
    if (ret != len) {
        printf("bme68x_i2c_dev_write: i2c_write_blocking(req=0x%x, len=%d) returned %d\n", reg_addr, len, ret);
        return ret;
    }
#endif

#ifdef BME68X_I2C_LOG_TRAFFIC
    printf("bme68x_i2c_dev_write(0x%0lx, len=%d) = ", reg_addr, len);
    for (size_t i = 0; i < len; i++)
    {
        printf("0x%lx ", reg_data[i]);
    }
    printf("\n");
#endif

    return BME68X_INTF_RET_SUCCESS;
}

static void bme68x_i2c_dev_delay_us(uint32_t period, void *intf_ptr)
{
    sleep_us(period);
}

int8_t bme68x_i2c_dev_init(struct bme68x_i2c_dev *dev, i2c_inst_t *i2c, uint8_t addr)
{
    struct bme68x_dev bme68x_dev = {
        .chip_id = BME68X_CHIP_ID,
        .intf_ptr = dev,
        .intf = BME68X_I2C_INTF,
        .read = bme68x_i2c_dev_read,
        .write = bme68x_i2c_dev_write,
        .delay_us = bme68x_i2c_dev_delay_us,
        .amb_temp = 25
    };

    dev->i2c = i2c;
    dev->addr = addr;
    dev->bme68x_dev = bme68x_dev;

    int8_t ret = bme68x_init(&dev->bme68x_dev);
    if (ret != BME68X_OK) {
        printf("bme68x_init returned %d\n", ret);
        return ret;
    }
    dev->bme68x_conf.odr = BME68X_ODR_NONE;
    dev->bme68x_conf.os_hum = BME68X_OS_1X;
    dev->bme68x_conf.os_pres = BME68X_OS_1X;
    dev->bme68x_conf.os_temp = BME68X_OS_1X;
    dev->bme68x_conf.filter = BME68X_FILTER_OFF;
//    dev->bme68x_conf.standby_time = BME68X_ODR_250_MS;

    ret = bme68x_set_conf(&dev->bme68x_conf, &dev->bme68x_dev);
    if (ret != BME68X_OK)
    {
        printf("bme68x_set_conf returned %d\n", ret);
        return ret;   
    }

    dev->bme68x_heatr_conf.enable = BME68X_ENABLE;
    dev->bme68x_heatr_conf.heatr_temp = 300;
    dev->bme68x_heatr_conf.heatr_dur = 100;
    ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &dev->bme68x_heatr_conf, &dev->bme68x_dev);
    if (ret != BME68X_OK)
    {
        printf("bme68x_set_heatr_conf returned %d\n", ret);
        return ret;   
    }

    dev->measurement_delay_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &dev->bme68x_conf, &dev->bme68x_dev);
    dev->measurement_delay_us += (dev->bme68x_heatr_conf.heatr_dur * 1000);

    return 0;
}

int8_t bme68x_i2c_dev_read_data(struct bme68x_i2c_dev *dev, struct bme68x_data *data)
{
    int8_t ret;

    uint8_t sensor_mode = BME68X_SLEEP_MODE;
    ret = bme68x_get_op_mode(&sensor_mode, &dev->bme68x_dev);
    if (ret) {
        printf("bme68x_i2c_dev_wait_read_data: bme68x_get_op_mode returned %d\n", ret);
        return ret;
    }

    if (sensor_mode == BME68X_SLEEP_MODE)
    {
        ret = bme68x_set_op_mode(BME68X_FORCED_MODE, &dev->bme68x_dev);
        if (ret != BME68X_OK)
        {
            printf("bme68x_set_op_mode(BME68X_FORCED_MODE) returned %d\n", ret);
            return ret;   
        }

        dev->bme68x_dev.delay_us(dev->measurement_delay_us, dev->bme68x_dev.intf_ptr);
    }
    else if (sensor_mode == BME68X_FORCED_MODE)
    {
        dev->bme68x_dev.delay_us(dev->measurement_delay_us, dev->bme68x_dev.intf_ptr);
    }
    
    uint8_t n_fields;
    ret = bme68x_get_data(BME68X_FORCED_MODE, data, &n_fields, &dev->bme68x_dev);
    if (ret && (ret != BME68X_W_NO_NEW_DATA))
    {
        printf("bme68x_get_data(BME68X_FORCED_MODE) returned %d\n", ret);
        return ret;
    }

    return 0;
}

int8_t bme68x_i2c_dev_get_conf(struct bme68x_i2c_dev *dev, struct bme68x_conf *bme68x_conf)
{
    memcpy(bme68x_conf, &dev->bme68x_conf, sizeof(struct bme68x_conf));

    return 0;
}

int8_t bme68x_i2c_dev_set_conf(struct bme68x_i2c_dev *dev, struct bme68x_conf *bme68x_conf)
{
    int8_t ret;

    ret = bme68x_set_conf(bme68x_conf, &dev->bme68x_dev);
    if (ret != BME68X_OK)
    {
        printf("bme68x_set_conf returned %d\n", ret);
        return ret;   
    }

    memcpy(&dev->bme68x_conf, bme68x_conf, sizeof(struct bme68x_conf));

    return 0;
}

int8_t bme68x_i2c_dev_set_heatr_conf(struct bme68x_i2c_dev *dev, struct bme68x_heatr_conf *bme68x_heatr_conf)
{
    int8_t ret;

    ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, bme68x_heatr_conf, &dev->bme68x_dev);
    if (ret != BME68X_OK)
    {
        printf("bme68x_set_heatr_conf returned %d\n", ret);
        return ret;   
    }

    memcpy(&dev->bme68x_heatr_conf, bme68x_heatr_conf, sizeof(struct bme68x_heatr_conf));

    return 0;
}

int8_t bme68x_i2c_dev_get_heatr_conf(struct bme68x_i2c_dev *dev, struct bme68x_heatr_conf *bme68x_heatr_conf)
{
    memcpy(bme68x_heatr_conf, &dev->bme68x_heatr_conf, sizeof(struct bme68x_heatr_conf));

    return 0;
}