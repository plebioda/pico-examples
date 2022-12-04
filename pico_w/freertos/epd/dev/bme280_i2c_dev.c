#include "bme280_i2c_dev.h"

#include <stdio.h>
#include <string.h>
// #define BME280_I2C_LOG_TRAFFIC

static BME280_INTF_RET_TYPE bme280_i2c_dev_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    struct bme280_i2c_dev *dev = (struct bme280_i2c_dev *)intf_ptr;
    int ret;

    ret = i2c_write_blocking(dev->i2c, dev->addr, &reg_addr, 1, true);
    if (ret != 1) {
        printf("bme280_i2c_dev_read: i2c_write_blocking(req=0x%x) returned %d\n", reg_addr, ret);
        return ret;
    }

    ret = i2c_read_blocking(dev->i2c, dev->addr, reg_data, len, false);
    if (ret != len) {
        printf("bme280_i2c_dev_read: i2c_read_blocking(req=0x%x, len=%d) returned %d\n", reg_addr, len, ret);
        return ret;
    }

#ifdef BME280_I2C_LOG_TRAFFIC
    printf("bme280_i2c_dev_read(0x%0lx, len=%d) = ", reg_addr, len);
    for (size_t i = 0; i < len; i++)
    {
        printf("0x%lx ", reg_data[i]);
    }
    printf("\n");
#endif

    return BME280_INTF_RET_SUCCESS;
}
#define USE_ONE_WRITE
static BME280_INTF_RET_TYPE bme280_i2c_dev_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    struct bme280_i2c_dev *dev = (struct bme280_i2c_dev *)intf_ptr;
    int ret;

#ifdef USE_ONE_WRITE
    uint8_t buff[256];
    buff[0] = reg_addr;
    memcpy(&buff[1], reg_data, len);
    ret = i2c_write_blocking(dev->i2c, dev->addr, buff, 1 + len, false);
    if (ret != len + 1) {
        printf("bme280_i2c_dev_write: i2c_write_blocking(req=0x%x) returned %d\n", reg_addr, ret);
        return ret;
    }
#else
    ret = i2c_write_blocking(dev->i2c, dev->addr, &reg_addr, 1, true);
    if (ret != 1) {
        printf("bme280_i2c_dev_write: i2c_write_blocking(req=0x%x) returned %d\n", reg_addr, ret);
        return ret;
    }

    ret = i2c_write_blocking(dev->i2c, dev->addr, reg_data, len, false);
    if (ret != len) {
        printf("bme280_i2c_dev_write: i2c_write_blocking(req=0x%x, len=%d) returned %d\n", reg_addr, len, ret);
        return ret;
    }
#endif

#ifdef BME280_I2C_LOG_TRAFFIC
    printf("bme280_i2c_dev_write(0x%0lx, len=%d) = ", reg_addr, len);
    for (size_t i = 0; i < len; i++)
    {
        printf("0x%lx ", reg_data[i]);
    }
    printf("\n");
#endif

    return BME280_INTF_RET_SUCCESS;
}

static void bme280_i2c_dev_delay_us(uint32_t period, void *intf_ptr)
{
    sleep_us(period);
}

int8_t bme280_i2c_dev_init(struct bme280_i2c_dev *dev, i2c_inst_t *i2c, uint8_t addr)
{
    struct bme280_dev bme280_dev = {
        .chip_id = BME280_CHIP_ID,
        .intf_ptr = dev,
        .intf = BME280_I2C_INTF,
        .read = bme280_i2c_dev_read,
        .write = bme280_i2c_dev_write,
        .delay_us = bme280_i2c_dev_delay_us,
    };

    dev->i2c = i2c;
    dev->addr = addr;
    dev->bme280_dev = bme280_dev;

    int8_t ret = bme280_init(&dev->bme280_dev);
    if (ret != BME280_OK) {
        printf("bme280_init returned %d\n", ret);
        return ret;
    }

    dev->bme280_dev.settings.osr_h = BME280_OVERSAMPLING_1X;
    dev->bme280_dev.settings.osr_p = BME280_OVERSAMPLING_1X;
    dev->bme280_dev.settings.osr_t = BME280_OVERSAMPLING_1X;
    dev->bme280_dev.settings.filter = BME280_FILTER_COEFF_OFF;
    dev->bme280_dev.settings.standby_time = BME280_STANDBY_TIME_250_MS;

    uint8_t settings = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

    ret = bme280_set_sensor_settings(settings, &dev->bme280_dev);
    if (ret != BME280_OK)
    {
        printf("bme280_set_sensor_settings returned %d\n", ret);
        return ret;
    }

    dev->measurement_delay_us = bme280_cal_meas_delay(&dev->bme280_dev.settings);
    printf("bme280_i2c_dev_init: measurement delay: %lu us\n", dev->measurement_delay_us);

    ret = bme280_set_sensor_mode(BME280_FORCED_MODE, &dev->bme280_dev);
    if (ret) {
        printf("bme280_set_sensor_mode(BME280_FORCED_MODE) returned %d\n", ret);
        return ret;
    }

    return 0;
}

int8_t bme280_i2c_dev_read_data(struct bme280_i2c_dev *dev, struct bme280_data *data)
{
    int8_t ret;

    uint8_t sensor_mode = BME280_SLEEP_MODE;
    ret = bme280_get_sensor_mode(&sensor_mode, &dev->bme280_dev);
    if (ret) {
        printf("bme280_i2c_dev_wait_read_data: bme280_get_sensor_mode returned %d\n", ret);
        return ret;
    }
    if (sensor_mode == BME280_SLEEP_MODE)
    {
        ret = bme280_set_sensor_mode(BME280_FORCED_MODE, &dev->bme280_dev);
        if (ret) {
            printf("bme280_i2c_dev_wait_read_data: bme280_set_sensor_mode(BME280_FORCED_MODE) returned %d\n", ret);
            return ret;
        }

        dev->bme280_dev.delay_us(2 * dev->measurement_delay_us, dev->bme280_dev.intf_ptr);
    }
    
    return bme280_get_sensor_data(BME280_ALL, data, &dev->bme280_dev);
}